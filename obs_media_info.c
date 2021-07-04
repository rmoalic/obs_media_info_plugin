#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <obs/obs.h>
#include <obs/obs-module.h>
#include <obs/util/bmem.h>
#include <obs/util/platform.h>
#include <obs/util/threading.h>
#include <obs/graphics/graphics.h>
#include "player_mpris_get_info.h"
#include "track_info.h"
#include "utils.h"

#define SETTING_SELECTED_PLAYER "SELECTED_PLAYER"
#define SETTING_NO_SELECTED_PLAYER "None"
#define SETTING_FALLBACK_SELECTED_PLAYER "FALLBACK_SELECTED_PLAYER"
#define SETTING_TEMPLATE "TEXT_TEMPLATE"
#define SETTING_TEMPLATE_DEFAULT "\"%title%\" - %artist%\n\
from %album%"
#define SETTING_TEXT_FIELD "TEXT_FIELD"


typedef struct source {
    bool live;
    uint32_t width;
    uint32_t height;
    pthread_t update_thread;
    bool end_update_thread;
    pthread_mutex_t* texture_mutex;
    gs_texture_t* texture;
    const char* selected_player;
    bool fallback_if_selected_player_not_running;
    const char* template;
    const char* text_field;
} obsmed_source;

static const char* obsmed_get_name(void* type_data) {
    return "Media infos";
}

static void update_obs_text_source(char* source_name, char* new_text) {
    obs_source_t* text_source = obs_get_source_by_name(source_name);
    if (text_source == NULL) return;
    obs_data_t* tdata = obs_data_create();
    if (tdata == NULL) return;

    obs_data_set_string(tdata, "text", new_text);
    obs_source_update(text_source, tdata);

    obs_data_release(tdata);
    obs_source_release(text_source);
}

static int strlen0(char* str) {
    if (str == NULL) return 0;
    return strlen(str);
}

static void apply_template(char* template, TrackInfo* track_info, char* ret, int nb_max) {
    char tmp[200];
    char* token;
    int nb_token = 0;
    const char sep[] = "%";
    ret[0] = '\0';

    strncpy(tmp, template, 199);
    tmp[199] = '\0';

    int tlen;
    token = strtok(tmp, sep);
    while (token != NULL) {
        if (strcmp(token, "title") == 0) {
            tlen = strlen0(track_info->title);
            if (nb_max - tlen <= 0) return;
            strncat(ret, track_info->title, tlen);
        } else if (strcmp(token, "artist") == 0) {
            tlen = strlen0(track_info->artist);
            if (nb_max - tlen <= 0) return;
            strncat(ret, track_info->artist, tlen);
        } else if (strcmp(token, "album") == 0) {
            tlen = strlen0(track_info->album);
            if (nb_max - tlen <= 0) return;
            strncat(ret, track_info->album, tlen);
        } else if (strcmp(token, "album_art_url") == 0) {
            tlen = strlen0(track_info->album_art_url);
            if (nb_max - tlen <= 0) return;
            strncat(ret, track_info->album_art_url, tlen);
        } else {
            tlen = strlen0(token);
            if (nb_max - tlen <= 0) return;
            strncat(ret, token, tlen);
        }
        nb_max -= tlen - 1;
        token = strtok(NULL, sep);
        nb_token = nb_token + 1;
    }

    if ((nb_token) % 2 != 0) {
        printf("Malformed template\n");
    }
}

static void* update_func(void* arg) {
    mpris_init();
    obsmed_source* source = arg;
    char* last_track_url = NULL;
    time_t last_update_time = time(NULL);
    TrackInfo* last_track = NULL;
    bool changed = false;

    while (! source->end_update_thread) {
        mpris_process(); // Get new data

        TrackInfo* current_track = NULL;
        if (strcmp(SETTING_NO_SELECTED_PLAYER, source->selected_player) == 0) {
            current_track = track_info_get_best_cantidate();
        } else {
            assert(source->selected_player != NULL);
            current_track = track_info_get_from_selected_player_fancy_name(source->selected_player);
            if (current_track == NULL && source->fallback_if_selected_player_not_running) {
                current_track = track_info_get_best_cantidate();
            }
        }

        if (current_track != last_track) {
            last_track = current_track;
            changed = true;
        }

        if (current_track != NULL && current_track->update_time > last_update_time) {
            changed = true;
            last_update_time = current_track->update_time;
        }

        if (changed && current_track != NULL) {
            if (current_track->album_art_url != NULL &&
                    (last_track_url == NULL ||
                     strcmp(last_track_url, current_track->album_art_url) != 0)
               ) {
                pthread_mutex_lock(source->texture_mutex);
                obs_enter_graphics();
                if (source->texture != NULL) gs_texture_destroy(source->texture);
                //TODO: syncronise texture and text updating
                source->texture = gs_texture_create_from_file(current_track->album_art_url); //TODO: texture from http only works with obs's ffmpeg backend not with imageMagic.
                if (source->texture == NULL) puts("error loading texture");
                obs_leave_graphics();
                pthread_mutex_unlock(source->texture_mutex);

                if (last_track_url != NULL) free(last_track_url);
                last_track_url = strdup(current_track->album_art_url);
                allocfail_print(last_track_url);
            }

            char text[200];
            printf("ICI\n");
            apply_template((char*)source->template, current_track, text, 199);
            update_obs_text_source((char*)source->text_field, text);
        }
        os_sleep_ms(500);
    }
    free(last_track_url);
    pthread_exit(NULL);
}

static void* obsmed_create(obs_data_t *settings, obs_source_t *source) {
    obsmed_source* data = bmalloc(sizeof(obsmed_source));
    allocfail_exit(data);

    data->width = 300;
    data->height = 300;
    data->texture = NULL;
    data->selected_player = obs_data_get_string(settings, SETTING_SELECTED_PLAYER);
    data->fallback_if_selected_player_not_running = obs_data_get_bool(settings, SETTING_FALLBACK_SELECTED_PLAYER);
    data->template = obs_data_get_string(settings, SETTING_TEMPLATE);
    data->text_field = obs_data_get_string(settings, SETTING_TEXT_FIELD);

    data->end_update_thread = false;
    data->texture_mutex = bmalloc(sizeof(pthread_mutex_t));
    allocfail_exit(data->texture_mutex);
    pthread_mutex_init(data->texture_mutex, NULL);
    if(pthread_create(&(data->update_thread), NULL, update_func, data)) {
        fprintf(stderr, "Error creating thread\n");
        return NULL;
    }

    return data;
}

static void obsmed_destroy(void* d) {
    obsmed_source* data = d;
    data->end_update_thread = true;
    pthread_join(data->update_thread, NULL);
    pthread_mutex_destroy(data->texture_mutex);
    bfree(data->texture_mutex);
    gs_texture_destroy(data->texture);
    bfree(data);
}

static uint32_t obsmed_get_width(void* data) {
    obsmed_source* d = data;
    return d->width;
}

static uint32_t obsmed_get_height(void* data) {
    obsmed_source* d = data;
    return d->height;
}


static void obsmed_update(void *data, obs_data_t *settings) {
    obsmed_source* d = data;

    d->selected_player = obs_data_get_string(settings, SETTING_SELECTED_PLAYER);
    d->fallback_if_selected_player_not_running = obs_data_get_bool(settings, SETTING_FALLBACK_SELECTED_PLAYER);
    d->template = obs_data_get_string(settings, SETTING_TEMPLATE);
    d->text_field = obs_data_get_string(settings, SETTING_TEXT_FIELD);
}
static void obsmed_get_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, SETTING_SELECTED_PLAYER, SETTING_NO_SELECTED_PLAYER);
    obs_data_set_default_bool(settings, SETTING_FALLBACK_SELECTED_PLAYER, false);
    obs_data_set_default_string(settings, SETTING_TEMPLATE, SETTING_TEMPLATE_DEFAULT);
    obs_data_set_default_string(settings, SETTING_TEXT_FIELD, "");
}

static bool add_sources_from_text_plugins(void* param, obs_source_t* source) {
    obs_property_t* p = param;
    const char* id = obs_source_get_id(source);

    if (strstr(id, "text") != NULL) {
        const char* name = obs_source_get_name(source);
        obs_property_list_add_string(p, name, name);
    }
    return true;
}

static obs_properties_t* obsmed_get_properties(void *data)
{
//    obsmed_source* d = data;
    obs_properties_t *props = obs_properties_create();

    obs_property_t* p = obs_properties_add_list(props, SETTING_SELECTED_PLAYER, obs_module_text("Selected player"), OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "None", SETTING_NO_SELECTED_PLAYER);
    int nb_players;
    TrackInfoPlayer** players = track_info_get_players(&nb_players);
    if (players != NULL) {
        for (int i = 0; i < nb_players; i++) {
            if (strstr(players[i]->fancy_name, "instance") != NULL) continue; // don't include instances of player. Their id is random
            obs_property_list_add_string(p, players[i]->fancy_name, players[i]->fancy_name);
        }
        free(players);
    }

    obs_properties_add_bool(props, SETTING_FALLBACK_SELECTED_PLAYER, obs_module_text("Fallback if selected player not running"));

    p = obs_properties_add_list(props, SETTING_TEXT_FIELD, obs_module_text("Text source name"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "", "");
    obs_enum_sources(add_sources_from_text_plugins, p);

    obs_properties_add_text(props, SETTING_TEMPLATE, obs_module_text("Text template"), OBS_TEXT_MULTILINE);
    return props;
}


static void obsmed_video_render(void *data, gs_effect_t *effect) {
     obsmed_source* d = data;

     if (pthread_mutex_trylock(d->texture_mutex) == 0) {
         obs_source_draw(d->texture, 0, 0, d->width, d->height, false);
         pthread_mutex_unlock(d->texture_mutex);
     }
}


static struct obs_source_info obs_media_info = {
    .id = "obs_media_info",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO,
    .create = obsmed_create,
    .destroy = obsmed_destroy,
    .video_render = obsmed_video_render,
    .get_name = obsmed_get_name,
    .get_width = obsmed_get_width,
    .get_height = obsmed_get_height,
    .update = obsmed_update,
    .get_defaults = obsmed_get_defaults,
    .get_properties = obsmed_get_properties,
    .icon_type = OBS_ICON_TYPE_CUSTOM,
};



OBS_DECLARE_MODULE()
//OBS_MODULE_USE_DEFAULT_LOCALE("obs_media_info", "en-US") //TODO: localisation
MODULE_EXPORT const char *obs_module_description(void)
{
    return "Plugin to display the cover artwork and basic informations on the currently playing sound";
}

bool obs_module_load(void)
{
    obs_register_source(&obs_media_info);
    return true;
}
