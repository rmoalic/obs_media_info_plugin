#include <stdbool.h>
#include <string.h>
#include <time.h>
// OBS
#include <obs.h>
#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>
#include <util/threading.h>
#include <graphics/graphics.h>
// END OBS
#include "player_info_get.h"
#include "track_info.h"
#include "utils.h"
#include "list.h"
#define LOG_PREFIX "[obs_media_info] "
#include "logging.h"

#define obs_module_text(x) #x

#define SETTING_SELECTED_PLAYER "SELECTED_PLAYER"
#define SETTING_NO_SELECTED_PLAYER "None"
#define SETTING_FALLBACK_SELECTED_PLAYER "FALLBACK_SELECTED_PLAYER"
#define SETTING_BLACKOUT_IF_NOT_PLAYING "BLACKOUT_NOT_PLAYING"
#define SETTING_TEMPLATE "TEXT_TEMPLATE"
#define SETTING_TEMPLATE_DEFAULT "\"%title%\" - %artist%\n\
from %album%"
#define SETTING_TEXT_FIELD "TEXT_FIELD"

typedef struct source {
    uint32_t width;
    uint32_t height;
    pthread_mutex_t* texture_mutex;
    gs_texture_t* texture;
    uint32_t texture_width;
    uint32_t texture_height;

    //config
    const char* selected_player;
    bool fallback_if_selected_player_not_running;
    bool blackout_if_not_playing;
    const char* template;
    const char* text_field;

    // thread
    char* last_track_url;
    time_t last_update_time;
    TrackInfo* last_track;
    bool changed;
} obsmed_source;


static list sources;
static pthread_mutex_t* sources_mutex;

static pthread_t update_thread;
static bool end_update_thread = false;

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
        log_warning("Malformed template\n");
    }
}

static bool update_track_is_different_from_last(obsmed_source* source, TrackInfo* track) {
    bool ret = false;

    if (track != source->last_track) {
        log_debug("track changed: %p != %p\n", (void*) track, (void*) source->last_track);
        source->last_track = track;
        ret = true;
    }

    if (track != NULL && track->update_time > source->last_update_time) {
        log_debug("track changed: update time\n");
        source->last_update_time = track->update_time;
        ret = true;
    }

    return ret;
}

static TrackInfo* update_get_current_track(obsmed_source* source) {
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

    return current_track;
}

static void update_source(obsmed_source* source) {
    TrackInfo* current_track = update_get_current_track(source);

    source->changed = update_track_is_different_from_last(source, current_track);
    if (! source->changed) return;

    if (current_track != NULL) {
        if (current_track->album_art != NULL) { //TODO: refactor (add an image parsing library for linux)
            pthread_mutex_lock(source->texture_mutex);
            obs_enter_graphics();
            if (source->texture != NULL) gs_texture_destroy(source->texture);

            source->texture_width = current_track->album_art_width;
            source->texture_height = current_track->album_art_height;
            source->texture = gs_texture_create(current_track->album_art_width, current_track->album_art_height, GS_RGBA, 1, (const uint8_t **) &(current_track->album_art), 0);

            if (source->texture == NULL) log_warning("error loading texture\n");
            obs_leave_graphics();
            pthread_mutex_unlock(source->texture_mutex);
        } else if (current_track->album_art_url == NULL && source->last_track_url != NULL) {
            pthread_mutex_lock(source->texture_mutex);
            obs_enter_graphics();
            if (source->texture != NULL) gs_texture_destroy(source->texture);

            source->texture = NULL;
            obs_leave_graphics();
            pthread_mutex_unlock(source->texture_mutex);

            efree(source->last_track_url);
            source->last_track_url = NULL;
        } else if (current_track->album_art_url != NULL &&
                   (source->last_track_url == NULL || strcmp(source->last_track_url, current_track->album_art_url) != 0)) {
            pthread_mutex_lock(source->texture_mutex);
            obs_enter_graphics();
            if (source->texture != NULL) gs_texture_destroy(source->texture);

            //TODO: syncronise texture and text updating
            source->texture = gs_texture_create_from_file(current_track->album_art_url); //TODO: texture from http only works with obs's ffmpeg backend not with imageMagic.

            if (source->texture == NULL) {
              log_warning("error loading texture\n");
              source->texture_width = 0;
              source->texture_height = 0;
            } else {
              source->texture_width = gs_texture_get_width(source->texture);
              source->texture_height = gs_texture_get_height(source->texture);
            }
            obs_leave_graphics();
            pthread_mutex_unlock(source->texture_mutex);

            efree(source->last_track_url);
            source->last_track_url = strdup(current_track->album_art_url);
            allocfail_print(source->last_track_url);
        }
        char text[200];
        apply_template((char*)source->template, current_track, text, 199);
        update_obs_text_source((char*)source->text_field, text);
    } else {
        if (source->blackout_if_not_playing) {
            // remove texture
            if (source->texture != NULL) {
                pthread_mutex_lock(source->texture_mutex);
                obs_enter_graphics();
                gs_texture_destroy(source->texture);
                source->texture = NULL;
                obs_leave_graphics();
                pthread_mutex_unlock(source->texture_mutex);
                efree(source->last_track_url);
                source->last_track_url = NULL;
            }
            // remove text
            update_obs_text_source((char*)source->text_field, "");
        }
    }
    source->changed = false;
}

static void* update_func(void* arg) {
    player_info_init();
    list* sources_lst = arg;

    while (! end_update_thread) {
        player_info_process(); // Get new data

        pthread_mutex_lock(sources_mutex);
        struct list_element* curr = *sources_lst;
        while (curr != NULL) {
            obsmed_source* source = curr->element;

            update_source(source);

            curr = curr->next;
        }
        pthread_mutex_unlock(sources_mutex);
        os_sleep_ms(500);
    }
    pthread_exit(NULL);
    return NULL;
}

static int sources_cmp(void* sa, void* sb) {
    return !(sa == sb); // 0 means true
}

static void* obsmed_create(obs_data_t *settings, obs_source_t *source) {
    obsmed_source* data = bmalloc(sizeof(obsmed_source));
    allocfail_exit(data);

    //config
    data->selected_player = obs_data_get_string(settings, SETTING_SELECTED_PLAYER);
    char* instance_nb = strstr(data->selected_player, ".instance"); // Instances of players have a random id, not usefull across reboot
    if (instance_nb != NULL) {
        *instance_nb = '\0';
    }
    data->fallback_if_selected_player_not_running = obs_data_get_bool(settings, SETTING_FALLBACK_SELECTED_PLAYER);
    data->blackout_if_not_playing = obs_data_get_bool(settings, SETTING_BLACKOUT_IF_NOT_PLAYING);
    data->template = obs_data_get_string(settings, SETTING_TEMPLATE);
    data->text_field = obs_data_get_string(settings, SETTING_TEXT_FIELD);
    //endconfig

    data->width = 300;
    data->height = 300;
    data->texture = NULL;
    data->texture_mutex = bmalloc(sizeof(pthread_mutex_t));
    data->texture_width = 0;
    data->texture_height = 0;
    allocfail_exit(data->texture_mutex);
    pthread_mutex_init(data->texture_mutex, NULL);


    if (list_size(sources) == 0) { // is first source
        log_info("starting update thread\n");
        if(pthread_create(&update_thread, NULL, update_func, &sources)) {
            log_error("Error creating thread\n");
            return NULL;
        }
    }
    pthread_mutex_lock(sources_mutex);
    //thread
    data->last_track_url = NULL;
    data->last_update_time = time(NULL);
    data->last_track = NULL;
    data->changed = false;
    //endthread

    list_prepend(&sources, data, sizeof(obsmed_source));
    pthread_mutex_unlock(sources_mutex);
    return data;
}

static void obsmed_destroy(void* d) {
    obsmed_source* data = d;

    pthread_mutex_lock(sources_mutex);
    list_remove(&sources, data, sources_cmp);
    pthread_mutex_unlock(sources_mutex);
    if (list_size(sources) == 0) {
        log_info("stopping update thread\n");
        end_update_thread = true;
        pthread_join(update_thread, NULL);
    }

    efree(data->last_track_url);
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
    d->blackout_if_not_playing = obs_data_get_bool(settings, SETTING_BLACKOUT_IF_NOT_PLAYING);
    d->template = obs_data_get_string(settings, SETTING_TEMPLATE);
    d->text_field = obs_data_get_string(settings, SETTING_TEXT_FIELD);

    log_debug("track changed: plugin settings update\n");
    d->changed = true;
}
static void obsmed_get_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, SETTING_SELECTED_PLAYER, SETTING_NO_SELECTED_PLAYER);
    obs_data_set_default_bool(settings, SETTING_FALLBACK_SELECTED_PLAYER, false);
    obs_data_set_default_bool(settings, SETTING_BLACKOUT_IF_NOT_PLAYING, false);
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
            obs_property_list_add_string(p, players[i]->fancy_name, players[i]->fancy_name);
        }
        efree(players);
    }

    obs_properties_add_bool(props, SETTING_FALLBACK_SELECTED_PLAYER, obs_module_text("Fallback to other players if the selected player is not running"));

    obs_properties_add_bool(props, SETTING_BLACKOUT_IF_NOT_PLAYING, obs_module_text("Blackout if no player is playing"));

    p = obs_properties_add_list(props, SETTING_TEXT_FIELD, obs_module_text("Text source name"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "", "");
    obs_enum_sources(add_sources_from_text_plugins, p);

    obs_properties_add_text(props, SETTING_TEMPLATE, obs_module_text("Text template"), OBS_TEXT_MULTILINE);
    return props;
}


static void center_texture_on_container(uint32_t container_width, uint32_t container_height, uint32_t texture_width, uint32_t texture_height, int* x, int* y, uint32_t* cx, uint32_t* cy) {
    if (texture_width == 0 || texture_height == 0) return;
    double tcx = (double) texture_width * ((double) container_width / ((double) texture_width));
    double tcy = (double) texture_height * ((double) container_height / ((double) texture_height));
    double texture_ratio = texture_width / (texture_height * 1.0);
    if (texture_ratio < 1.0) {
        tcx *= texture_ratio;        
    } else {
        tcy /= texture_ratio;
    }

    *cx = 0.5 + tcx;
    *cy = 0.5 + tcy;

    if (*cy < container_height) {
        *y = (container_height - *cy) / 2;
    } else {
        *y = 0;
    }
    if (*cx < container_width) {
        *x = (container_width - *cx) / 2;
    } else {
        *x = 0;
    }
}

static void obsmed_video_render(void *data, gs_effect_t *effect) {
     obsmed_source* d = data;

     if (pthread_mutex_trylock(d->texture_mutex) == 0) {
         if (d->texture != NULL) {
            int x, y;
            uint32_t cx, cy;
            if (d->texture_width > 0 && d->texture_height > 0) {
                center_texture_on_container(d->width, d->height, d->texture_width, d->texture_height, &x, &y, &cx, &cy);
                obs_source_draw(d->texture, x, y, cx, cy, false);
            }
         }
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
    log_info("loading\n");
    list_init(&sources);

    sources_mutex = bmalloc(sizeof(pthread_mutex_t));
    allocfail_exit(sources_mutex);
    pthread_mutex_init(sources_mutex, NULL);
    return true;
}

void obs_module_unload(void)
{
    pthread_mutex_destroy(sources_mutex);
    bfree(sources_mutex);
}
