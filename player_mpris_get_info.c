#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <dbus/dbus.h>
#include "player_info_get.h"
#include "track_info.h"
#include "utils.h"
#define LOG_PREFIX "[obs_media_info] "
#include "logging.h"

#define NB_PLAYER_MAX 10
static const char MPRIS_NAME_START[] = "org.mpris.MediaPlayer2";

struct current_track_extra_info {
  char* track_url;
};

static DBusConnection* dbus_connection = NULL;
static TrackInfo current_track = {0};
static struct current_track_extra_info current_track_extra = {0};
static bool playing = false;

struct track_info_has_updates {
  bool has_url;
  bool has_title;
  bool has_art_url;
  bool has_artist;
  bool has_album;
};


static int decodeURIComponent (char *sSource, char *sDest) { // https://stackoverflow.com/a/20437049
    assert(sSource != NULL);
    assert(sDest != NULL);
    int nLength;
    for (nLength = 0; *sSource; nLength++) {
        if (*sSource == '%' && sSource[1] && sSource[2] && isxdigit(sSource[1]) && isxdigit(sSource[2])) {
            sSource[1] -= sSource[1] <= '9' ? '0' : (sSource[1] <= 'F' ? 'A' : 'a')-10;
            sSource[2] -= sSource[2] <= '9' ? '0' : (sSource[2] <= 'F' ? 'A' : 'a')-10;
            sDest[nLength] = 16 * sSource[1] + sSource[2];
            sSource += 3;
            continue;
        }
        sDest[nLength] = *sSource++;
    }
    sDest[nLength] = '\0';
    return nLength;
}

#define implodeURIComponent(url) decodeURIComponent(url, url)

static char* correct_art_url(const char* url) {
    assert(url != NULL);
    static const char spotify_old[] = "https://open.spotify.com/image/";
    static const char spotify_new[] = "https://i.scdn.co/image/";
    static const char file_url[] = "file://";
    size_t url_len = strlen(url);
    char* ret;

    if (url_len <= sizeof(file_url) - 1) {
        ret = strdup(url);
        allocfail_return_null(ret);
    } else if (strncmp(spotify_old, url, sizeof(spotify_old) - 1) == 0) {
        ret = malloc((1 + url_len - (sizeof(spotify_old) - sizeof(spotify_new) - 2)) * sizeof(char));
        allocfail_return_null(ret);
        sprintf(ret, "%s%s", spotify_new, url + (sizeof(spotify_old) - 1));
    } else if (strncmp(file_url, url, sizeof(file_url) - 1) == 0) {
        ret = malloc((3 + url_len - sizeof(file_url) - 1) * sizeof(char));
        allocfail_return_null(ret);
        sprintf(ret, "%s", url + (sizeof(file_url) - 1));
        implodeURIComponent(ret);
    } else {
        ret = strdup(url);
        allocfail_return_null(ret);
    }
    return ret;
}

static bool update_data_if_diff(char** store, const char* new) {
    bool ret = false;
    if (new == NULL) {
        if (*store != NULL) {
            free(*store);
            *store = NULL;
            ret = true;
        }
    } else if (*store == NULL || estrcmp(*store, new) != 0) {
        if (*store != NULL) free(*store);
        *store = strdup(new);
        allocfail_return_null(*store);
        ret = true;
    }
    return ret;
}

static void parse_MetaData(DBusMessageIter* iter, bool* updated_data_ret, struct track_info_has_updates* has_updates) {
    int current_type;
    DBusMessageIter sub, subsub;
    const char* property_name = NULL;

    while ((current_type = dbus_message_iter_get_arg_type (iter)) != DBUS_TYPE_INVALID) {

        switch (current_type) {
        case DBUS_TYPE_DICT_ENTRY: {
            dbus_message_iter_recurse(iter, &sub);
            parse_MetaData(&sub, updated_data_ret, has_updates);

        } break;
        case DBUS_TYPE_STRING: {
            dbus_message_iter_get_basic(iter, &property_name);

        } break;
        case DBUS_TYPE_VARIANT: {
            if (property_name == NULL) {
                log_error("Error: Encontered property before its name, ignoring\n");
                continue;
            }
            if (strcmp(property_name, "xesam:title") == 0) {
                char* title;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &title);
                if (update_data_if_diff(&(current_track.title), title)) {
                    *updated_data_ret = true;
                }
                has_updates->has_title = true;
            } else if (strcmp(property_name, "mpris:artUrl") == 0) {
                char* url;
                char* correct_url;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &url);
                correct_url = correct_art_url(url);
                if (update_data_if_diff(&(current_track.album_art_url), correct_url)) {
                    *updated_data_ret = true;
                }
                has_updates->has_art_url = true;
            } else if (strcmp(property_name, "xesam:artist") == 0) {
                char* artist;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_recurse(&sub, &subsub);
                dbus_message_iter_get_basic(&subsub, &artist);
                if (update_data_if_diff(&(current_track.artist), artist)) {
                    *updated_data_ret = true;
                }
                has_updates->has_artist = true;
            } else if (strcmp(property_name, "xesam:album") == 0) {
                char* album;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &album);
                if (update_data_if_diff(&(current_track.album), album)) {
                    *updated_data_ret = true;
                }
                has_updates->has_album = true;
            } else if (strcmp(property_name, "xesam:url") == 0) {
                char* url;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &url);
                if (update_data_if_diff(&current_track_extra.track_url, url)) {
                    *updated_data_ret = true;
                }
                has_updates->has_url = true;
            } else {
                log_debug("ignored: %s\n", property_name);
            }
        } break;
        default:
            log_error("parse error\n");
        }
        dbus_message_iter_next(iter);
    }
}

static void parse_array(DBusMessageIter* iter, bool* updated_data_ret) {
    int current_type;
    DBusMessageIter sub, subsub;
    const char* property_name = NULL;

    while ((current_type = dbus_message_iter_get_arg_type (iter)) != DBUS_TYPE_INVALID) {

        switch (current_type) {
        case DBUS_TYPE_DICT_ENTRY: {
            dbus_message_iter_recurse(iter, &sub);
            parse_array(&sub, updated_data_ret);
        } break;
        case DBUS_TYPE_STRING: {
            dbus_message_iter_get_basic(iter, &property_name);
            log_debug("%s\n", property_name);
        } break;
        case DBUS_TYPE_VARIANT: {
            if (property_name == NULL) {
                log_error("Error: Encontered property before its name, ignoring\n");
                continue;
            }
            if (strcmp(property_name, "PlaybackStatus") == 0) {
                char* status;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &status);
                log_debug("Status: %s\n", status);
                if (strcmp(status, "Playing") == 0) {
                    playing = true;
                } else {
                    playing = false;
                }
            } else if (strcmp(property_name, "Metadata") == 0) {
                struct track_info_has_updates has_updates = {0};

                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_recurse(&sub, &subsub);
                parse_MetaData(&subsub, updated_data_ret, &has_updates);

                if (! has_updates.has_url)      update_data_if_diff(&(current_track_extra.track_url) , NULL);

                if (! has_updates.has_title)    update_data_if_diff(&(current_track.title)                , NULL);
                if (! has_updates.has_art_url)  update_data_if_diff(&(current_track.album_art_url)        , NULL);
                if (! has_updates.has_artist)   update_data_if_diff(&(current_track.artist)               , NULL);
                if (! has_updates.has_album)    update_data_if_diff(&(current_track.album)                , NULL);

                if (! has_updates.has_title && has_updates.has_url) {
                  char* name = strrchr(current_track_extra.track_url, '/');
                  if (name != NULL && update_data_if_diff(&(current_track.title), name + 1)) {
                    *updated_data_ret = true;
                  }
                }
            } else {
                log_debug("not handled %s\n", property_name);
            }
        } break;
        default:
            log_error("parse error\n");
        }
        dbus_message_iter_next(iter);
    }
}

static DBusHandlerResult my_message_handler_mpris(DBusConnection *connection, DBusMessage *message, void *user_data) {
    DBusError error;
    DBusMessageIter iter, sub;
    int current_type;
    bool updated_data = false;
    bool old_playing = playing;

    dbus_error_init(&error);

    const char* property_name;
    const char* player = dbus_message_get_sender(message);
    log_debug("\n\nreceived message from %s\n", player);


    dbus_message_iter_init (message, &iter);
    while ((current_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_INVALID) {

        switch (current_type) {
        case DBUS_TYPE_STRING: {
            dbus_message_iter_get_basic(&iter, &property_name);
            log_debug("%s\n", property_name);
        } break;
        case DBUS_TYPE_ARRAY: {
            if (strcmp(property_name, "org.mpris.MediaPlayer2.Player") == 0) {
                int c = dbus_message_iter_get_element_count(&iter);
                if (c == 0) break; // if empty array, skip

                dbus_message_iter_recurse(&iter, &sub);

                parse_array(&sub, &updated_data);
            }
        } break;
        default:
            log_error("parse error\n");
        }
        dbus_message_iter_next(&iter);
    }

    if (updated_data) {
        track_info_register_track_change(player, current_track);
        playing = true;
        track_info_register_state_change(player, playing); // if a track changes, it is playing (vlc)
    } else if (playing != old_playing) {
        track_info_register_state_change(player, playing);
    }

    #ifdef DEBUG
    track_info_print_players();
    #endif

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult my_message_handler_dbus(DBusConnection *connection, DBusMessage *message, void *user_data) {
    const char *name;
    const char *old_name;
    const char *new_name;
    DBusError error;
    dbus_error_init (&error);

    if (!dbus_message_get_args(message, &error,
                               DBUS_TYPE_STRING, &name,
                               DBUS_TYPE_STRING, &old_name,
                               DBUS_TYPE_STRING, &new_name,
                               DBUS_TYPE_INVALID)) {
        if (dbus_error_is_set(&error)) {
            log_error("Error while reading new names signal (%s)\n", error.message);
            dbus_error_free(&error);
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    bool registering_name = old_name != NULL && strlen(old_name) == 0;

    if (strncmp(MPRIS_NAME_START, name, sizeof(MPRIS_NAME_START) - 1) == 0) { // if mpris name
        if (registering_name) {
            log_info("registration of %s as %s\n", new_name, name);
            track_info_register_player(new_name, name);
        } else {
            log_info("unregistering of %s as %s\n", old_name, name);
            track_info_unregister_player(old_name);
        }
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult my_message_handler(DBusConnection *connection, DBusMessage *message, void *user_data) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_HANDLED;

    const char* dest = dbus_message_get_destination(message);
    const char* my_name = dbus_bus_get_unique_name(connection);
    if (dest != NULL && strcmp(dest, my_name) != 0) {
        log_warning("Received a message for %s I am %s, ignoring\n", dest, my_name);
        return ret;
    }

    const char* path = dbus_message_get_path(message);
    if (path == NULL) return ret;
    const char* member = dbus_message_get_member(message);

    if (strcmp(path, "/org/mpris/MediaPlayer2") == 0) {
        ret = my_message_handler_mpris(connection, message, user_data);
    } else if (strcmp(path, "/org/freedesktop/DBus") == 0 && strcmp(member, "NameOwnerChanged") == 0) {
        ret = my_message_handler_dbus(connection, message, user_data);
    } else {
        log_warning("NOT HANDLED: message with path %s and member %s\n", path, member);
        ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    return ret;
}

static char* mydbus_get_name_owner(DBusConnection* dbus, const char* name) {
    DBusMessage* msg, *resp;
    DBusMessageIter imsg;
    DBusError error;
    char* ret = NULL;
    dbus_error_init (&error);
    msg = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetNameOwner");
    dbus_message_iter_init_append(msg, &imsg);
    dbus_message_iter_append_basic(&imsg, DBUS_TYPE_STRING, &name);

    resp = dbus_connection_send_with_reply_and_block(dbus, msg, -1, &error); //TODO: remove blocking call
    if (dbus_error_is_set(&error)) {
        log_error("Error while reading owner name (%s)\n", error.message);
    } else {
        char* resp_name;

        if (! dbus_message_get_args(resp, &error,
                                    DBUS_TYPE_STRING, &resp_name,
                                    DBUS_TYPE_INVALID)) {
            if (dbus_error_is_set(&error)) {
                log_error("Error while reading owner name (%s)\n", error.message);
            }
        } else {
            ret = strdup(resp_name);
            allocfail_print(ret);
        }
        dbus_message_unref(resp);
    }
    dbus_message_unref(msg);
    return ret;
}

static void mydbus_register_names(DBusConnection* dbus)
{
    DBusMessage* msg;
    DBusPendingCall* resp_pending = NULL;

    msg = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

    if (! dbus_connection_send_with_reply(dbus, msg, &resp_pending, -1)) {
        log_error("Out Of Memory!\n");
    }

    dbus_pending_call_block(resp_pending); //TODO: remove blocking call

    if (dbus_pending_call_get_completed(resp_pending)) {
        DBusMessage* resp;
        resp = dbus_pending_call_steal_reply(resp_pending);
        int current_type;
        DBusMessageIter iter, iter2;

        dbus_message_iter_init (resp, &iter);
        if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
            dbus_message_iter_recurse(&iter, &iter2); // Array of String
            while ((current_type = dbus_message_iter_get_arg_type (&iter2)) != DBUS_TYPE_INVALID) {
                if (current_type == DBUS_TYPE_STRING) {
                    char* name;
                    dbus_message_iter_get_basic(&iter2, &name);
                    if (strncmp(MPRIS_NAME_START, name, sizeof(MPRIS_NAME_START) - 1) == 0) { // if mpris name
                        char* unique_name = mydbus_get_name_owner(dbus, name);
                        track_info_register_player(unique_name, name);
                        free(unique_name);
                    }
                }
                dbus_message_iter_next(&iter2);
            }
        }
        dbus_message_unref(resp);
    }
    dbus_message_unref(msg);
}


static DBusConnection* mydbus_init_session()
{
    DBusConnection *cbus = NULL;
    DBusError error;

    dbus_error_init(&error);
    cbus = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
        log_error("Error getting Bus: %s\n", error.message);
        dbus_error_free(&error);
    }
    if (cbus == NULL) {
        log_error("Error getting Bus: cbus is null\n");
    }

    /*dbus_bus_request_name(cbus, "fr.polms.obs_get_mpris_info", DBUS_NAME_FLAG_REPLACE_EXISTING, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Error while claiming name (%s)\n", error.message);
        dbus_error_free(&error);
        }*/
    log_info("My dbus name is %s\n", dbus_bus_get_unique_name(cbus));

    return cbus;
}

static void mydbus_add_matches(DBusConnection* dbus) {
    DBusError error;
    dbus_error_init (&error);
    dbus_bus_add_match(dbus, "type='signal', interface='org.freedesktop.DBus.Properties',member='PropertiesChanged', path='/org/mpris/MediaPlayer2'", &error);

    if (dbus_error_is_set(&error)) {
        log_error("Error while adding match (%s)\n", error.message);
        dbus_error_free(&error);
    }

    dbus_bus_add_match(dbus, "type='signal', interface='org.freedesktop.DBus', member='NameOwnerChanged', path='/org/freedesktop/DBus'", &error);

    if (dbus_error_is_set(&error)) {
        log_error("Error while adding match (%s)\n", error.message);
        dbus_error_free(&error);
    }

    dbus_connection_add_filter(dbus, my_message_handler, NULL, dbus_free);
    dbus_connection_flush(dbus);
}


void player_info_init() {
    log_info("Initialising");
    dbus_connection = mydbus_init_session();

    mydbus_add_matches(dbus_connection);
    track_info_init();

    mydbus_register_names(dbus_connection);
}

int player_info_process() {
    DBusDispatchStatus remains;

    do {
        dbus_connection_read_write_dispatch(dbus_connection, 500);
        remains = dbus_connection_get_dispatch_status(dbus_connection);
        if (remains == DBUS_DISPATCH_NEED_MEMORY) {
            log_warning("Need more memory to read dbus messages\n");
        }
    } while (remains != DBUS_DISPATCH_COMPLETE);

    return 0;
}
