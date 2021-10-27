#ifndef TRACK_INFO_H
#define TRACK_INFO_H 1

#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <sal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct track_info_player {
    _Field_z_ const char* name;
    _Field_z_ const char* fancy_name;
} TrackInfoPlayer;

typedef struct track_info {
    _Field_z_ char* title;
    _Field_z_ char* artist;
    _Field_z_ char* album;
    _Field_z_ char* album_art_url;
    _Field_size_part_(4, album_art_width * album_art_height)
	uint8_t* album_art;
	uint32_t  album_art_width;
	uint32_t  album_art_height;
    time_t update_time;
} TrackInfo;

void track_info_init(void);
void track_info_struct_free(_In_ TrackInfo* ti);
void track_info_struct_init(_Inout_ TrackInfo* ti);
void track_info_print_players();
void track_info_register_track_change(_In_z_ const char* name, _In_ TrackInfo track);
void track_info_register_state_change(_In_z_ const char* name, _In_ bool playing);
TrackInfo* track_info_get_best_cantidate(void);
TrackInfo* track_info_get_from_selected_player_fancy_name(_In_z_ const char* player_fancy_name);
void track_info_print(_In_ TrackInfo ti);
void track_info_unregister_player(_In_z_ const char* name);
void track_info_register_player(_In_z_ const char* name, _In_z_ const char* fancy_name);
TrackInfoPlayer** track_info_get_players(_Out_ int* ret_nb);


#ifdef __cplusplus
} //end extern "C"
#endif
#endif
