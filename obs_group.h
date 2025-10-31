#ifndef OBS_GROUP_H
#define OBS_GROUP_H 1

#include <obs.h>

obs_scene_t* source_get_parent_scene(obs_source_t* source);
obs_source_t* source_get_group(obs_source_t* source);
obs_sceneitem_t* source_get_group_sceneitem(obs_source_t* source);

#endif
