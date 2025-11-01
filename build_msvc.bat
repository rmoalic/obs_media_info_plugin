@echo off

set CFLAGS=/std:c17 /utf-8 /Zi
set CXXFLAGS=/std:c++17 /utf-8 /EHsc /Zi /await
set INCLUDES=/I . /I obs_studio/libobs /I obs_studio/deps/w32-pthreads
set LIBS=obs.lib w32-pthreads.lib

del *.obj

rc.exe ressource.rc
cl.exe %CXXFLAGS% %INCLUDES% /c player_windows_uwp_get_info.cpp
cl.exe %CFLAGS% %INCLUDES% /c list.c obs_media_info.c track_info.c obs_group.c

link.exe /DEBUG:FULL %LIBS% ressource.res list.obj obs_media_info.obj track_info.obj obs_group.obj player_windows_uwp_get_info.obj /DLL /OUT:obs_media_info.dll

