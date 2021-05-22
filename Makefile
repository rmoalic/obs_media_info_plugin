all: lib

lib: obs_spotify_info.so

obs_spotify_info.so: obs_spotify_info.o player_mpris_get_info.o track_info.o
	clang -shared track_info.o player_mpris_get_info.o obs_spotify_info.o -o obs_spotify_info.so

obs_spotify_info.o: obs_spotify_info.c
	clang -Wall -c -fPIC obs_spotify_info.c

track_info.o: track_info.c track_info.h
	clang -Wall -Wextra -fPIC -c track_info.c

player_mpris_get_info.o: player_mpris_get_info.c track_info.o
	clang -c -fPIC `pkg-config --cflags --libs dbus-1` track_info.o player_mpris_get_info.c


clean:
	rm *.o
	rm *.so
