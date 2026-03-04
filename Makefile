CC=gcc
WARNINGS=-Wall -Wextra -Wmissing-prototypes -Wshadow -Wno-unused-parameter
CFLAGS=-g  -MMD -MP -pedantic -I/usr/include/obs `pkgconf --cflags libobs`  $(WARNINGS)

.PHONY: all clean

all: lib

lib: obs_media_info.so

obs_media_info.so: obs_media_info.o obs_group.o player_mpris_get_info.o track_info.o list.o image_loader.o
	$(CC) `pkgconf --libs dbus-1 libobs libavcodec libavformat libswscale libavutil` -shared $^ -o $@

player_mpris_get_info.o: player_mpris_get_info.c
	$(CC) $(CFLAGS) -c -fPIC `pkgconf --cflags dbus-1` $< -o $@

image_loader.o: image_loader.c
	$(CC) $(CFLAGS) -c -fPIC `pkgconf --cflags libavcodec libavformat libswscale libavutil` $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@

clean:
	rm *.o *.d *.so

-include $(wildcard *.d)
