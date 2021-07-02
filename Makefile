CC=clang
CFLAGS=-g -Wall -Wextra -Wno-unused-parameter -MMD -MP -pedantic


.PHONY: all clean

all: lib

lib: obs_media_info.so

obs_media_info.so: obs_media_info.o player_mpris_get_info.o track_info.o list.o
	$(CC) `pkg-config --libs dbus-1 libobs` -shared $^ -o $@

player_mpris_get_info.o: player_mpris_get_info.c
	$(CC) $(CFLAGS) -c -fPIC `pkg-config --cflags dbus-1` $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@

clean:
	rm *.o *.d *.so

-include $(wildcard *.d)
