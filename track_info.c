#include <stdio.h>
#include <stdlib.h>
#include "track_info.h"

void track_info_free(TrackInfo* ti) {
    if (ti->artist != NULL) free(ti->artist);
    if (ti->album != NULL) free(ti->album);
    if (ti->title != NULL) free(ti->title);
    if (ti->album_art_url != NULL) free(ti->album_art_url);
    ti->artist = NULL;
    ti->album = NULL;
    ti->title = NULL;
    ti->album_art_url = NULL;
}

void track_info_print(TrackInfo ti) {
    printf("%s - \"%s\" from %s, art: %s\n", ti.artist, ti.title, ti.album, ti.album_art_url);
}
