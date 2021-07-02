#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <obs/obs-module.h>
#include <obs/util/bmem.h>
#include <obs/util/platform.h>
#include <obs/util/threading.h>
#include <obs/graphics/graphics.h>
#include "track_info.h"
#include "utils.h"

void mpris_init();
int mpris_process();

typedef struct source {
    bool live;
    uint32_t width;
    uint32_t height;
    pthread_t update_thread;
    bool end_update_thread;
    pthread_mutex_t* texture_mutex;
    gs_texture_t* texture;
} obsmed_source;

const char* obsmed_get_name(void* type_data) {
    return "Media infos";
}

static void update_obs_text_source(char* source_name, char* new_text) {
    obs_source_t* text_source = obs_get_source_by_name(source_name);
    obs_data_t* tdata = obs_data_create();

    obs_data_set_string(tdata, "text", new_text);
    obs_source_update(text_source, tdata);

    obs_data_release(tdata);
    obs_source_release(text_source);
}

void* update_func(void* arg) {
    obsmed_source* source = arg;
    char* last_track_url = NULL;
    time_t last_update_time = time(NULL);
    TrackInfo* last_track = NULL;
    bool changed = false;

    while (! source->end_update_thread) {
        mpris_process(); // Get new data

        TrackInfo* current_track = track_info_get_best_cantidate();

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
            snprintf(text, 199, "%s\n%s\n%s", current_track->title, current_track->album, current_track->artist);
            update_obs_text_source("toto", text);
        }
        os_sleep_ms(500);
    }
    free(last_track_url);
    pthread_exit(NULL);
}

void* obsmed_create(obs_data_t *settings, obs_source_t *source) {
    obsmed_source* data = bmalloc(sizeof(obsmed_source));
    allocfail_exit(data);
    mpris_init();

    data->width = 300;
    data->height = 300;
    data->texture = NULL;

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

void obsmed_destroy(void* d) {
    obsmed_source* data = d;
    data->end_update_thread = true;
    pthread_join(data->update_thread, NULL);
    pthread_mutex_destroy(data->texture_mutex);
    bfree(data->texture_mutex);
    gs_texture_destroy(data->texture);
    bfree(data);
}

uint32_t obsmed_get_width(void* data) {
    obsmed_source* d = data;
    return d->width;
}

uint32_t obsmed_get_height(void* data) {
    obsmed_source* d = data;
    return d->height;
}


static obs_properties_t* obsmed_get_properties(void *data)
{
//    obsmed_source* d = data;
    obs_properties_t *props = obs_properties_create();

    obs_property_t* player_list = obs_properties_add_list(props, "Preferred player", obs_module_text("Preferred player"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(player_list, "None", "None");
    int nb_players;
    TrackInfoPlayer** players = track_info_get_players(&nb_players);
    for (int i = 0; i < nb_players; i++) {
        obs_property_list_add_string(player_list, players[i]->fancy_name, players[i]->fancy_name);
    }
    free(players);
    return props;
}


void obsmed_video_render(void *data, gs_effect_t *effect) {
     obsmed_source* d = data;

     if (pthread_mutex_trylock(d->texture_mutex) == 0) {
         obs_source_draw(d->texture, 0, 0, d->width, d->height, false);
         pthread_mutex_unlock(d->texture_mutex);
     }
}


struct obs_source_info obs_media_info = {
    .id = "obs_media_info",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO,
    .create = obsmed_create,
    .destroy = obsmed_destroy,
    .video_render = obsmed_video_render,
    .get_name = obsmed_get_name,
    .get_width = obsmed_get_width,
    .get_height = obsmed_get_height,
    /*.update = obsmed_update,
    .get_defaults = obsmed_get_defaults,*/
    .get_properties = obsmed_get_properties,
    .icon_type = OBS_ICON_TYPE_CUSTOM,
};



OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs_media_info", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
    return "Plugin to display the cover artwork and basic informations on the currently playing sound";
}

bool obs_module_load(void)
{
    obs_register_source(&obs_media_info);
    return true;
}
