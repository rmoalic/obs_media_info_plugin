all: lib

lib: obs_media_info.so

obs_media_info.so: obs_media_info.o player_mpris_get_info.o track_info.o list.o
	clang -g -shared track_info.o player_mpris_get_info.o obs_media_info.o list.o -o obs_media_info.so

obs_media_info.o: obs_media_info.c
	clang -g -Wall -c -fPIC obs_media_info.c

track_info.o: track_info.c track_info.h
	clang -g -Wall -Wextra -fPIC -c track_info.c

player_mpris_get_info.o: player_mpris_get_info.c track_info.o
	clang -g -c -fPIC `pkg-config --cflags --libs dbus-1` track_info.o player_mpris_get_info.c

standalone: standalone.c track_info.o player_mpris_get_info.o list.o
	clang -g -Wall -lcurl  `pkg-config --cflags --libs dbus-1` track_info.o player_mpris_get_info.o list.o standalone.c -o standalone

list.o: list.c list.h
	clang -g -c list.c

clean:
	rm *.o
	rm *.so
	rm standalone
