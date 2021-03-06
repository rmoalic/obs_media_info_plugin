#ifndef TRACK_INFO_H
#define TRACK_INFO_H 1

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct track_info_player {
    const char* name;
    const char* fancy_name;
} TrackInfoPlayer;

typedef struct track_info {
    char* title;
    char* artist;
    char* album;
    char* album_art_url;
	uint8_t* album_art;
	uint32_t  album_art_width;
	uint32_t  album_art_height;
    time_t update_time;
} TrackInfo;

void track_info_init(void);
void track_info_struct_free(TrackInfo* ti);
void track_info_struct_init(TrackInfo* ti);
void track_info_print(TrackInfo ti);
void track_info_register_track_change(const char* player, TrackInfo track);
void track_info_register_state_change(const char* player, bool playing);
TrackInfo* track_info_get_best_cantidate(void);
TrackInfo* track_info_get_from_selected_player_fancy_name(const char* player_fancy_name);
void track_info_print_players(void);
void track_info_unregister_player(const char* player);
void track_info_register_player(const char* player, const char* player_fancy_name);
TrackInfoPlayer** track_info_get_players(int* ret_nb);


#ifdef __cplusplus
} //end extern "C"
#endif
#endif
