# obs_media_info

> **WARNING** WIP

> **WARNING** This project only works on linux

This project is an attempt at making a [OBS Studio](https://obsproject.com/) plugin.

The plugin display the current playing track info (title, album, artist) and artwork.

Data is collected though the [MPRIS 2](https://specifications.freedesktop.org/mpris-spec/latest/) interface so a lot of players should be supported (VLC, Spotify, Firefox, ...).

## Screenshot
![Screenshot of obs with the plugin installed](screen.png)


## Build

``` shell
$ apt update
$ apt install build-essential pkgconf # requiring make gcc pkgconf
$ apt install libdbus-1-dev libobs-dev

% make
```

## Install

Place the `obs_media_plugin.so` in the obs plugins directory.

``` shell
% mkdir -p /home/$HOME/.config/obs-studio/plugins/obs_media_info/bin/64bit
% cp obs_media_plugin.so /home/$HOME/.config/obs-studio/plugins/obs_media_info/bin/64bit/
```

Or in the systemwide directory `/usr/share/obs/obs-plugins/` (distribution dependant. Archlinux: `/usr/lib/obs-plugins/`).

``` shell
$ cp obs_media_plugin.so /usr/share/obs/obs-plugins/
```

## TODO

[] Handle different artwork ratio (ex: Youtube videos)
[] Handle missing artwork (or partial info in general)
[] Select prefered player

## Note

If using Firefox make sure `media.hardwaremediakeys.enabled` is set to `true` (default)
