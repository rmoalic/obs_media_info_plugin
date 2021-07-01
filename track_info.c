#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "track_info.h"
#include "list.h"

#define efree(ptr) if (ptr != NULL) { free(ptr); ptr = NULL; }
#define estrcmp(a, b) ((a == NULL || b == NULL) ? 0 : strcmp(a, b))

typedef struct track_info_per_player {
    const char* player;
    TrackInfo track;
    bool playing;
} TrackInfoPerPlayer;

list players;

static int players_name_cmp(TrackInfoPerPlayer* a, TrackInfoPerPlayer* b) {
    return strcmp(a->player, b->player);
}

void track_info_init() {
    list_init(&players);
}
TrackInfo* track_info_get_best_cantidate() {
    struct list_element* curr = players;
    TrackInfo* best_candidate = NULL;

    while (curr != NULL) {
        TrackInfoPerPlayer* e = curr->element;

        if (e->playing) {
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

static void track_info_dup(TrackInfo t, TrackInfo* ret) {
    ret->album = strdup(t.album);
    ret->artist = strdup(t.artist);
    ret->title = strdup(t.title);
    ret->album_art_url = strdup(t.album_art_url);
    ret->update_time = t.update_time;
}

void track_info_free(TrackInfo* ti) {
    efree(ti->artist);
    efree(ti->album);
    efree(ti->title);
    efree(ti->album_art_url);
}

static TrackInfoPerPlayer* track_info_get_for_player(const char* player) {
    TrackInfoPerPlayer* track_info = NULL;
    TrackInfoPerPlayer h = {.player = player};
    if (list_search(&players, &h, (list_cmpfunc) players_name_cmp, (void**) &track_info)) {
        printf("Found already registered player %s\n", player);
    } else {
        track_info = malloc(sizeof(TrackInfoPerPlayer));
        track_info->player = strdup(player);
        printf("Appened %s to list\n", player);
        list_prepend(&players, track_info, sizeof(TrackInfoPerPlayer));
    }
    return track_info;
}

void track_info_register_track_change(const char* player, TrackInfo track) {
    TrackInfoPerPlayer* track_info = track_info_get_for_player(player);

    track_info_dup(track, &(track_info->track));
}

void track_info_register_state_change(const char* player, bool playing) {
    TrackInfoPerPlayer* track_info = track_info_get_for_player(player);

    track_info->playing = playing;
}

void track_info_print_players() {
    struct list_element* curr = players;

    int i = 0;
    while (curr != NULL) {
        TrackInfoPerPlayer* e = curr->element;
        printf("Player %d: %s\n", i, e->player);
        printf("> ");
        track_info_print(e->track);
        printf("playing: %d\n\n", e->playing);

        i = i + 1;
        curr = curr->next;
    }
}

void track_info_print(TrackInfo ti) {
    printf("%s - \"%s\" from %s, art: %s\n", ti.artist, ti.title, ti.album, ti.album_art_url);
}
