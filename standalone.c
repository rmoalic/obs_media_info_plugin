#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
//#include "player_mpris_get_info.h"
#include "track_info.h"

extern TrackInfo current_track;
void mpris_init();
int mpris_process();

static bool http_get(const char* url) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (! curl) {
        fprintf(stderr, "No curl\n");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return false;
    }

    curl_easy_cleanup(curl);
    return true;
}

int main(int argc, char* argv[], char* env[]) {
    FILE* out_text;
    FILE* out_picture;

    mpris_init();

    char* last_track = strdup("");
    current_track.title = NULL;
    while (true) {
        mpris_process();
        if (current_track.title != NULL && strcmp(last_track, current_track.title) != 0) {
//            free(last_track);
            last_track = strdup(current_track.title);
            printf("changed track: %s - \"%s\" from %s\n", current_track.artist, current_track.title, current_track.album);
        }
        //sleep(50);
    }


    return EXIT_SUCCESS;
}
