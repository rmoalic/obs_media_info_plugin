#include <stdio.h>
#include <stdlib.h>
#include "track_info.h"

#define efree(ptr) if (ptr != NULL) { free(ptr); ptr = NULL; }

void track_info_free(TrackInfo* ti) {
    efree(ti->artist);
    efree(ti->album);
    efree(ti->title);
    efree(ti->album_art_url);
}

void track_info_print(TrackInfo ti) {
    printf("%s - \"%s\" from %s, art: %s\n", ti.artist, ti.title, ti.album, ti.album_art_url);
}
