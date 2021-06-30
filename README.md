# obs_media_info

> **WARNING** WIP

> **WARNING** This project only works on linux

This project is an attempt at making a [OBS Studio](https://obsproject.com/) plugin.

The plugin display the current playing track info (title, album, artist) and artwork.

Data is collected though the [MPRIS 2](https://specifications.freedesktop.org/mpris-spec/latest/) interface so a lot of players should be supported (VLC, Spotify, Firefox, ...). But their comportement vary alot and a lot of edge cases are not supported, particularly when using multiples players at the same time.

## Screenshot
![Screenshot of obs with the plugin installed](screen.png)

## TODO

[] Handle different artwork ratio (ex: Youtube videos)
[] Handle missing artwork (or partial info in general)
[] Select prefered player
[] Handle player overlap

## Note

If using Firefox make sure `media.hardwaremediakeys.enabled` is set to `true` (default)
