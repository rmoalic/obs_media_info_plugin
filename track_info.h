#ifndef TRACK_INFO_H
#define TRACK_INFO_H 1

#include <time.h>

typedef struct track_info {
    char* title;
    char* artist;
    char* album;
    char* album_art_url;
    time_t update_time;
} TrackInfo;

void track_info_free(TrackInfo* ti);
void track_info_print(TrackInfo ti);

#endif
