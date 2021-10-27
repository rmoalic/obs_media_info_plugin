@echo off

set CFLAGS=/std:c17 /utf-8 /Zi /analyze
set CXXFLAGS=/std:c++17 /utf-8 /EHsc /Zi
set INCLUDES=/I . /I obs_studio/libobs
set LIBS=obs.lib w32-pthreads.lib

del *.obj

cl.exe %CXXFLAGS% %INCLUDES% /c player_windows_uwp_get_info.cpp
cl.exe %CFLAGS% %INCLUDES% /c list.c obs_media_info.c track_info.c

link.exe /DEBUG:FULL %LIBS% list.obj obs_media_info.obj track_info.obj player_windows_uwp_get_info.obj /DLL /OUT:obs_media_info.dll

copy obs_media_info.* C:\Users\robin\Desktop\OBS-Studio-27.0.1-Full-x64\obs-plugins\64bit\
