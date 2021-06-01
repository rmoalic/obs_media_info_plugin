#include <stdbool.h>
#include <string.h>
#include <obs/obs-module.h>
#include <obs/util/bmem.h>
#include <obs/graphics/graphics.h>
#include "track_info.h"

extern TrackInfo current_track;
void mpris_init();
int mpris_process();

typedef struct source {
    bool live;
    uint32_t width;
    uint32_t height;
} obspot_source;

const char* obspot_get_name(void* type_data) {
    return "Spotify infos";
}

void* obspot_create(obs_data_t *settings, obs_source_t *source) {
    obspot_source* data = bmalloc(sizeof(obspot_source));

    data->width = 300;
    data->height = 300;

    mpris_init();
    return data;
}

void obspot_destroy(void* data) {
    bfree(data);
}

uint32_t obspot_get_width(void* data) {
    obspot_source* d = data;
    return d->width;
}

uint32_t obspot_get_height(void* data) {
    obspot_source* d = data;
    return d->height;
}


static obs_properties_t* obspot_get_properties(void *data)
{
    obspot_source* d = data;
    obs_properties_t *props = obs_properties_create();

    obs_properties_add_font(props, "font", obs_module_text("Font"));
    obs_properties_add_text(props, "text", obs_module_text("Text"), OBS_TEXT_MULTILINE);

    return props;
}


void obspot_video_render(void *data, gs_effect_t *effect) {
     mpris_process();
     obspot_source* d = data;
     static gs_texture_t* texture = NULL;
     static char* last_track_url = "";

     if (current_track.album_art_url != NULL && strcmp(last_track_url, current_track.album_art_url) != 0) {
         //if (last_track_url != NULL) free(last_track_url);
         if (texture != NULL) gs_texture_destroy(texture);
         texture = gs_texture_create_from_file(current_track.album_art_url);
         last_track_url = strdup(current_track.album_art_url);
     }

     obs_source_draw(texture, 0, 0, d->width, d->height, false);

     //gs_texture_destroy(texture);


     char text2[100];
     snprintf(text2, 99, "%s\n%s\n%s", current_track.title, current_track.album, current_track.artist);
     obs_source_t* text = obs_get_source_by_name("toto");
     obs_data_t* tdata = obs_data_create();
     obs_data_set_string(tdata, "text", text2);
     obs_source_update(text, tdata);
     obs_data_release(tdata);
     obs_source_release(text);
}


struct obs_source_info obs_spotify_info = {
    .id = "obs_spotify_info",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO,
    .create = obspot_create,
    .destroy = obspot_destroy,
    .video_render = obspot_video_render,
    .get_name = obspot_get_name,
    .get_width = obspot_get_width,
    .get_height = obspot_get_height,
    /*.update = obspot_update,
    .get_defaults = obspot_get_defaults,*/
    .get_properties = obspot_get_properties,
    .icon_type = OBS_ICON_TYPE_CUSTOM,
};



OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs_sportify_info", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
    return "Test plugin";
}

bool obs_module_load(void)
{
    obs_register_source(&obs_spotify_info);
    return true;
}
