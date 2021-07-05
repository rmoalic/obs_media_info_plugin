CC=gcc
WARNINGS=-Wall -Wextra -Wmissing-prototypes -Wshadow -Wno-unused-parameter
CFLAGS=-g  -MMD -MP -pedantic `pkgconf --cflags libobs`  $(WARNINGS)

.PHONY: all clean

all: lib

lib: obs_media_info.so

obs_media_info.so: obs_media_info.o player_mpris_get_info.o track_info.o list.o
	$(CC) `pkgconf --libs dbus-1 libobs` -shared $^ -o $@

player_mpris_get_info.o: player_mpris_get_info.c
	$(CC) $(CFLAGS) -c -fPIC `pkgconf --cflags dbus-1` $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@

clean:
	rm *.o *.d *.so

-include $(wildcard *.d)
