#include "obs_group.h"
#include <string.h>

struct scene_group_parents {
    obs_scene_t* scene;
    obs_source_t* group;
    obs_sceneitem_t* item;
};

struct search_params {
    const obs_source_t* source;

    struct scene_group_parents ret;
};

static bool scenes_enum_items2(obs_scene_t *scene, obs_sceneitem_t *item, void *data) {
    struct search_params* params = data;
    obs_source_t* source = obs_sceneitem_get_source(item);

    if (source == params->source) {
        obs_scene_t* parent_scene = obs_sceneitem_get_scene(item);
        obs_source_t* parent_source = obs_scene_get_source(parent_scene);

        params->ret.group = obs_source_get_ref(parent_source);
        return false;
    }
    return true;
}

static bool scenes_enum_items(obs_scene_t *scene, obs_sceneitem_t *item, void *data) {
    struct search_params* params = data;
    obs_source_t* source = obs_sceneitem_get_source(item);

    if (obs_sceneitem_is_group(item)) {
        obs_sceneitem_group_enum_items(item, scenes_enum_items2, data);
        if (params->ret.group) {
            params->ret.scene = obs_scene_get_ref(scene);
            obs_sceneitem_addref(item);
            params->ret.item = item;
            return false;
        }
    } else {
        if (source == params->source) {
            params->ret.scene = obs_scene_get_ref(scene);
            params->ret.group = NULL;
            params->ret.item = NULL;
            return false;
        }
    }
    return true;
}

static bool scenes_enum(void* data, obs_source_t* source) {
    //struct search_params* params = data;
    obs_scene_t* scene = obs_scene_from_source(source);

    obs_scene_enum_items(scene, scenes_enum_items, data);
    return true;
}

static struct scene_group_parents source_get_group_and_scene(obs_source_t* source) {
    struct search_params params = {
        .source = source,
        .ret = {0}
    };

    obs_enum_scenes(scenes_enum, &params);

    return params.ret;
}

obs_scene_t* source_get_parent_scene(obs_source_t* source) {
    // if this is too slow, cache the result and connect the refresh signal of the source to invalidate the cache.

    struct scene_group_parents sgp = source_get_group_and_scene(source);

    obs_sceneitem_release(sgp.item);
    obs_source_release(sgp.group);

    return sgp.scene;
}

obs_source_t* source_get_group(obs_source_t* source) {
    // if this is too slow, cache the result and connect the refresh signal of the source to invalidate the cache.

    struct scene_group_parents sgp = source_get_group_and_scene(source);

    obs_scene_release(sgp.scene);
    obs_sceneitem_release(sgp.item);

    return sgp.group;
}

obs_sceneitem_t* source_get_group_sceneitem(obs_source_t* source) {
    // if this is too slow, cache the result and connect the refresh signal of the source to invalidate the cache.

    struct scene_group_parents sgp = source_get_group_and_scene(source);

    obs_scene_release(sgp.scene);
    obs_source_release(sgp.group);

    return sgp.item;
}
