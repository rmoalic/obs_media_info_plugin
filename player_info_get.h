#ifndef player_info_header
#define player_info_header 1
#ifdef __cplusplus
extern "C" {
#endif

void player_info_init(void);
int player_info_process(void);
void player_info_close(void);

#ifdef __cplusplus
} //end extern "C"
#endif

#endif
