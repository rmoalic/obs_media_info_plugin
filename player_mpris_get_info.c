#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <dbus/dbus.h>
#include "track_info.h"

#define NB_PLAYER_MAX 10
static const char* MPRIS_NAME_START = "org.mpris.MediaPlayer2";
static const char* DBUS_NAME_START = ":";

DBusConnection* dbus;
TrackInfo current_track;
bool playing;

static int decodeURIComponent (char *sSource, char *sDest) { // https://stackoverflow.com/a/20437049
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
    static const char* spotify_old = "https://open.spotify.com/image/";
    static const char* spotify_new = "https://i.scdn.co/image/";
    static const char* file_url = "file://";
    char* ret;
    if (strncmp(spotify_old, url, strlen(spotify_old)) == 0) {
        ret = malloc(100 * sizeof(char)); // TODO: correct size
        sprintf(ret, "%s%s", spotify_new, url + strlen(spotify_old));
    } else if (strncmp(file_url, url, strlen(file_url)) == 0) {
        ret = malloc(100 * sizeof(char)); // TODO: correct size
        sprintf(ret, "%s", url + strlen(file_url));
        implodeURIComponent(ret); //TODO: file url decode on windows
    } else {
        ret = strdup(url);
    }
    return ret;
}

static bool update_data_if_diff(char** store, const char* new) {
    bool ret = false;

    if (*store == NULL || strcmp(*store, new) != 0) {
        if (*store != NULL) free(*store);
        *store = strdup(new);
        ret = true;
    }
    return ret;
}

static void parse_MetaData(DBusMessageIter* iter, bool* updated_data_ret) {
    int current_type;
    DBusMessageIter sub, subsub;
    const char* property_name = NULL;

    while ((current_type = dbus_message_iter_get_arg_type (iter)) != DBUS_TYPE_INVALID) {

        switch (current_type) {
        case DBUS_TYPE_DICT_ENTRY: {
            dbus_message_iter_recurse(iter, &sub);
            parse_MetaData(&sub, updated_data_ret);

        } break;
        case DBUS_TYPE_STRING: {
            dbus_message_iter_get_basic(iter, &property_name);

        } break;
        case DBUS_TYPE_VARIANT: {
            if (strcmp(property_name, "xesam:title") == 0) {
                char* title;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &title);
                if (update_data_if_diff(&(current_track.title), title)) {
                    *updated_data_ret = true;
                }
            } else if (strcmp(property_name, "mpris:artUrl") == 0) {
                char* url;
                char* correct_url;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &url);
                correct_url = correct_art_url(url);
                if (update_data_if_diff(&(current_track.album_art_url), correct_url)) {
                    *updated_data_ret = true;
                }
            } else if (strcmp(property_name, "xesam:artist") == 0) {
                char* artist;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_recurse(&sub, &subsub);
                dbus_message_iter_get_basic(&subsub, &artist);
                if (update_data_if_diff(&(current_track.artist), artist)) {
                    *updated_data_ret = true;
                }
            }else if (strcmp(property_name, "xesam:album") == 0) {
                char* album;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &album);
                if (update_data_if_diff(&(current_track.album), album)) {
                    *updated_data_ret = true;
                }
            } else {
                printf("ignored: %s\n", property_name);
            }
        } break;
        default:
            fprintf(stderr, "parse error 2\n");
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
            printf("%s\n", property_name);
        } break;
        case DBUS_TYPE_VARIANT: {
            if (strcmp(property_name, "PlaybackStatus") == 0) {
                char* status;
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_get_basic(&sub, &status);
                printf("Status: %s\n", status);
                if (strcmp(status, "Playing") == 0) {
                    playing = true;
                } else {
                    playing = false;
                }
            } else if (strcmp(property_name, "Metadata") == 0) {
                dbus_message_iter_recurse(iter, &sub);
                dbus_message_iter_recurse(&sub, &subsub);
                parse_MetaData(&subsub, updated_data_ret);
            } else {
                fprintf(stderr, "not handled %s\n", property_name);
            }
        } break;
        default:
            fprintf(stderr, "parse error 2\n");
        }
        dbus_message_iter_next(iter);
    }
}

static DBusHandlerResult my_message_handler_mpris(DBusConnection *connection, DBusMessage *message, void *user_data) {
    DBusError error;
    DBusMessageIter iter, sub, subsub;
    int current_type;
    bool updated_data = false;
    bool old_playing = playing;

    dbus_error_init(&error);

    const char* property_name;
    const char* player = dbus_message_get_sender(message);
    printf("\n\nreceived message from %s\n", player);


    dbus_message_iter_init (message, &iter);
    while ((current_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_INVALID) {

        switch (current_type) {
        case DBUS_TYPE_STRING: {
            dbus_message_iter_get_basic(&iter, &property_name);
            printf("%s\n", property_name);
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
            fprintf(stderr, "parse error\n");
        }
        dbus_message_iter_next(&iter);
    }

    if (updated_data) {
        current_track.update_time = time(NULL);
        track_info_register_track_change(player, current_track);
        playing = true;
        track_info_register_state_change(player, playing); // if a track changes, it is playing (vlc)
    } else if (playing != old_playing) {
        track_info_register_state_change(player, playing);
    }

    track_info_print_players();

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult my_message_handler_dbus(DBusConnection *connection, DBusMessage *message, void *user_data) {
    const char *name;
    const char *old_name;
    const char *new_name;

    if (!dbus_message_get_args(message, NULL,
                               DBUS_TYPE_STRING, &name,
                               DBUS_TYPE_STRING, &old_name,
                               DBUS_TYPE_STRING, &new_name,
                               DBUS_TYPE_INVALID)) {
        puts("error");
    }
    bool registering_name = old_name != NULL && strlen(old_name) == 0;
    if (registering_name) {
        printf("registration of %s as %s\n", new_name, name);
    } else {
        printf("unregistering of %s as %s\n", old_name, name);
    }

    if (strncmp(name, MPRIS_NAME_START, strlen(MPRIS_NAME_START)) == 0) { // if mpris name
        if (registering_name) {
            track_info_register_player(new_name, name);
        } else {
            track_info_unregister_player(old_name);
        }
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult my_message_handler(DBusConnection *connection, DBusMessage *message, void *user_data) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_HANDLED;

    const char* path = dbus_message_get_path(message);
    if (path == NULL) return ret;

    if (strcmp(path, "/org/mpris/MediaPlayer2") == 0) {
        ret = my_message_handler_mpris(connection, message, user_data);
    } else if (strcmp(path, "/org/freedesktop/DBus") == 0) {
        ret = my_message_handler_dbus(connection, message, user_data);
    } else {
        printf("UNHANDLED: message with path %s\n", path);
    }

    return ret;
}

static char* mydbus_get_name_owner(const char* name) {
    DBusMessage* msg, *resp;
    DBusMessageIter imsg;
    DBusPendingCall* resp_pending = NULL;
    char* ret;

    msg = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetNameOwner");
    dbus_message_iter_init_append(msg, &imsg);
    dbus_message_iter_append_basic(&imsg, DBUS_TYPE_STRING, &name);

    if (! dbus_connection_send_with_reply(dbus, msg, &resp_pending, -1)) {
        fprintf(stderr, "Out Of Memory!\n");
    }

    dbus_pending_call_block(resp_pending); //TODO: remove blocking call

    if (dbus_pending_call_get_completed(resp_pending)) {
        resp = dbus_pending_call_steal_reply(resp_pending);
        char* name;

        if (!dbus_message_get_args(resp, NULL,
                                   DBUS_TYPE_STRING, &name,
                                   DBUS_TYPE_INVALID)) {
            puts("error");
        }
        ret = strdup(name);
    }
    dbus_message_unref(msg);
    dbus_message_unref(resp);

    return ret;
}

static void mydbus_register_names()
{
    DBusMessage* msg, *resp;
    DBusPendingCall* resp_pending = NULL;

    msg = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

    if (! dbus_connection_send_with_reply(dbus, msg, &resp_pending, -1)) {
        fprintf(stderr, "Out Of Memory!\n");
    }

    dbus_pending_call_block(resp_pending); //TODO: remove blocking call

    if (dbus_pending_call_get_completed(resp_pending)) {
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
                    if (strncmp(name, MPRIS_NAME_START, strlen(MPRIS_NAME_START)) == 0) { // if mpris name
                        char* unique_name = mydbus_get_name_owner(name);
                        track_info_register_player(unique_name, name);
                        free(unique_name);
                    }
                }
                dbus_message_iter_next(&iter2);
            }
        }
    }
    dbus_message_unref(msg);
    dbus_message_unref(resp);
}


static DBusConnection* mydbus_init_session()
{
    DBusConnection *cbus = NULL;
    DBusError error;

    dbus_error_init(&error);
    cbus = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "%s", error.message);
        dbus_error_free(&error);
    }

    /*dbus_bus_request_name(cbus, "fr.polms.obs_get_mpris_info", DBUS_NAME_FLAG_REPLACE_EXISTING, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Error while claiming name (%s)\n", error.message);
        dbus_error_free(&error);
        }*/
    printf("My dbus name is %s\n", dbus_bus_get_unique_name(cbus));

    return cbus;
}

static void mydbus_add_matches(DBusConnection* dbus) {
    DBusError error;
    dbus_bus_add_match(dbus, "type='signal', interface='org.freedesktop.DBus.Properties',member='PropertiesChanged', path='/org/mpris/MediaPlayer2'", &error);

    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Error while adding match (%s)\n", error.message);
        dbus_error_free(&error);
    }

    dbus_bus_add_match(dbus, "type='signal', interface='org.freedesktop.DBus', member='NameOwnerChanged', path='/org/freedesktop/DBus'", &error);

    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Error while adding match (%s)\n", error.message);
        dbus_error_free(&error);
    }

    dbus_connection_add_filter(dbus, my_message_handler, NULL, dbus_free);
}


void mpris_init() {
    dbus = mydbus_init_session();

    mydbus_add_matches(dbus);
    track_info_init();

    mydbus_register_names();
}

int mpris_process() {
    return dbus_connection_read_write_dispatch(dbus, 0);
}
/*
int main(int argc, char *argv[])
{
    mpris_init();

    while (mpris_process()) {}

    return EXIT_SUCCESS;
}
*/
