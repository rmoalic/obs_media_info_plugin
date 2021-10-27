#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "track_info.h"
#include "list.h"
#include "utils.h"
#define LOG_PREFIX "[obs_media_info] "
#include "logging.h"

typedef struct track_info_per_player {
    TrackInfoPlayer player;
    TrackInfo track;
    bool playing;
    bool updated_once;
} TrackInfoPerPlayer;

static list players;

static int players_name_cmp(_In_ TrackInfoPerPlayer* a, _In_ TrackInfoPerPlayer* b) {
    return strcmp(a->player.name, b->player.name);
}

void track_info_init() {
    list_init(&players);
}

TrackInfo* track_info_get_best_cantidate() {
    struct list_element* curr = players;
    TrackInfo* best_candidate = NULL;

    while (curr != NULL) {
        TrackInfoPerPlayer* e = curr->element;

        if (e->updated_once && e->playing) {
            if (best_candidate == NULL) {
                best_candidate = &(e->track);
            } else {
                if (best_candidate->update_time < e->track.update_time) {
                    best_candidate = &(e->track);
                }
            }
        }
        curr = curr->next;
    }
    return best_candidate;
}

TrackInfo* track_info_get_from_selected_player_fancy_name(_In_z_ const char* player_fancy_name) {
    struct list_element* curr = players;
    TrackInfo* best_candidate = NULL;
    int player_name_len = strlen(player_fancy_name);

    while (curr != NULL) {
        TrackInfoPerPlayer* e = curr->element;
        // check for player instances ex: org.mpris.MediaPlayer2.vlc.instance372027
        if (strncmp(e->player.fancy_name, player_fancy_name, player_name_len) == 0) {
            if (e->updated_once &&  e->playing) {
                if (best_candidate == NULL) {
                    best_candidate = &(e->track);
                } else {
                    if (best_candidate->update_time < e->track.update_time) {
                        best_candidate = &(e->track);
                    }
                }
            }
        }
        curr = curr->next;
    }
    return best_candidate;
}

TrackInfoPlayer** track_info_get_players(_Out_ int* ret_nb) {
    struct list_element* curr = players;
    int nb_player = list_size(players);

    TrackInfoPlayer** ret = malloc(sizeof(TrackInfoPlayer*) * nb_player);
    allocfail_return_null(ret);

    *ret_nb = 0;
    while (curr != NULL) {
        TrackInfoPerPlayer* e = curr->element;
        ret[*ret_nb] = &(e->player);
        curr = curr->next;
        *ret_nb = *ret_nb + 1;
    }
    assert(nb_player == *ret_nb);
    return ret;
}

static void track_info_dup(_In_ TrackInfo t, _Inout_ TrackInfo* ret) {
    ret->album = strdup(t.album);
    ret->artist = strdup(t.artist);
    ret->title = strdup(t.title);
    ret->album_art_url = strdup(t.album_art_url);
    
    if (t.album_art != NULL) {
        int size = t.album_art_width * t.album_art_height * 4;
        assert(size % 4 == 0);
        ret->album_art = malloc(size * sizeof(uint8_t));
        memcpy(ret->album_art, t.album_art, size);
        ret->album_art_width = t.album_art_width;
        ret->album_art_height = t.album_art_height;
    }

    ret->update_time = t.update_time;
}

void track_info_struct_free(_In_ TrackInfo* ti) {
    efree(ti->artist);
    efree(ti->album);
    efree(ti->title);
    efree(ti->album_art_url);
    efree(ti->album_art);
}

void track_info_struct_init(_Inout_ TrackInfo* ti) {
    ti->artist = NULL;
    ti->album = NULL;
    ti->title = NULL;
    ti->album_art_url = NULL;
    ti->album_art = NULL;
    ti->album_art_width = 0;
    ti->album_art_height = 0;
    ti->update_time = 0;
}

void track_info_register_player(_In_z_ const char* name, _In_z_ const char* fancy_name){
    TrackInfoPerPlayer* track_info_per_player = malloc(sizeof(TrackInfoPerPlayer));
    allocfail_return(track_info_per_player);
    track_info_per_player->player.name = strdup(name);
    allocfail_return(track_info_per_player->player.name);
    track_info_per_player->player.fancy_name = strdup(fancy_name);
    allocfail_return(track_info_per_player->player.fancy_name);
    track_info_struct_init(&(track_info_per_player->track));
    track_info_per_player->playing = false;
    track_info_per_player->updated_once = false;
    list_prepend(&players, track_info_per_player, sizeof(TrackInfoPerPlayer));
}

void track_info_unregister_player(_In_z_ const char* name) {
    TrackInfoPerPlayer h = {.player.name = name};
    list_remove(&players, &h, (list_cmpfunc) players_name_cmp);
}

static TrackInfoPerPlayer* track_info_get_for_player(_In_z_ const char* name) {
    TrackInfoPerPlayer* track_info = NULL;
    TrackInfoPerPlayer h = {.player.name = name};
    if (list_search(&players, &h, (list_cmpfunc) players_name_cmp, (void**) &track_info)) {
        log_debug("Found already registered player %s (%s)\n", name, track_info->player.fancy_name);
    } else {
        log_warning("Unknown player %s\n", name);
    }
    return track_info;
}

void track_info_register_track_change(_In_z_ const char* name, _In_ TrackInfo track) {
    TrackInfoPerPlayer* track_info = track_info_get_for_player(name);
    if (track_info == NULL) return;

    track_info->updated_once = true;
    track_info_dup(track, &(track_info->track));
    track_info->track.update_time = time(NULL);
}

void track_info_register_state_change(_In_z_ const char* name, _In_ bool playing) {
    TrackInfoPerPlayer* track_info = track_info_get_for_player(name);
    if (track_info == NULL) return;

    track_info->playing = playing;
    track_info->track.update_time = time(NULL);
}

void track_info_print_players() {
    struct list_element* curr = players;

    int i = 0;
    while (curr != NULL) {
        TrackInfoPerPlayer* e = curr->element;
        printf("Player %d: %s (%s) updated_once: %d\n", i, e->player.name, e->player.fancy_name, e->updated_once);
        printf("> ");
        track_info_print(e->track);
        printf("playing: %d\n\n", e->playing);

        i = i + 1;
        curr = curr->next;
    }
}

void track_info_print(_In_ TrackInfo ti) {
    printf("%s - \"%s\" from %s, art: %s\n", ti.artist, ti.title, ti.album, ti.album_art_url);
}
