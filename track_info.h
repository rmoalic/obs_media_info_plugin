#ifndef TRACK_INFO_H
#define TRACK_INFO_H 1

#include <time.h>
#include <stdbool.h>

typedef struct track_info {
    char* title;
    char* artist;
    char* album;
    char* album_art_url;
    time_t update_time;
} TrackInfo;

void track_info_init();
void track_info_free(TrackInfo* ti);
void track_info_print(TrackInfo ti);
void track_info_register_track_change(const char* player, TrackInfo track);
void track_info_register_state_change(const char* player, bool playing);
TrackInfo* track_info_get_best_cantidate();
void track_info_print_players();

#endif
