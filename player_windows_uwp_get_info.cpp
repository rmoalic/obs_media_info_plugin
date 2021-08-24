#define UNICODE

#pragma comment(lib, "windowsapp")

#include <iostream>
#include <algorithm>

#include "player_info_get.h"
#include "track_info.h"
#include "utils.h"
#define LOG_PREFIX "[obs_media_info] "
#include "logging.h"

#include <winrt/base.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.Collections.h>


using namespace winrt;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;

using namespace std;

void update_players_registration();

static GlobalSystemMediaTransportControlsSessionManager session_manager { nullptr };
vector<string> registered_players;

static void handle_session_change(GlobalSystemMediaTransportControlsSessionManager session_manager_l, SessionsChangedEventArgs  const& args) {
    update_players_registration();
}

static void handle_media_property_change(GlobalSystemMediaTransportControlsSession session, MediaPropertiesChangedEventArgs const& arg) {
    TrackInfo current_track;
    track_info_struct_init(&current_track);
    bool playing;

    GlobalSystemMediaTransportControlsSessionMediaProperties media_properties {nullptr};

    winrt::hstring t = session.SourceAppUserModelId();
    std::string player_t = winrt::to_string(t);
    const char* player = player_t.c_str();

    media_properties = session.TryGetMediaPropertiesAsync().get();

    if (media_properties == nullptr) return;

    auto info = session.GetPlaybackInfo();
    if (info == nullptr) return;


    std::string title = winrt::to_string(media_properties.Title());
    current_track.title = (char*) title.c_str();
    std::string artist = winrt::to_string(media_properties.Artist());
    current_track.artist = (char*) artist.c_str();
    std::string album = winrt::to_string(media_properties.AlbumTitle());
    current_track.album = (char*) album.c_str();


    auto thumbnail = media_properties.Thumbnail();
    com_array<uint8_t> pixel_data_detached;
    uint8_t* data = NULL;
    if (thumbnail != nullptr) {
        auto stream = thumbnail.OpenReadAsync().get();
        auto decoder = BitmapDecoder::CreateAsync(stream).get();
        auto transform = BitmapTransform();
        uint32_t width, height;
        
        if (player_t == "Spotify.exe") { // The spotify player has branding on the thumbnail, cropping it out
            static const int Wcrop = 34;
            static const int Hcrop = 66;
            BitmapBounds spotify_bound;
            width = decoder.PixelWidth() - 2 * Wcrop;
            height = decoder.PixelHeight() - Hcrop;

            spotify_bound.X = Wcrop;
            spotify_bound.Y = 0;
            spotify_bound.Width = width;
            spotify_bound.Height = height;

            transform.Bounds(spotify_bound);

        } else {
            width = decoder.PixelWidth();
            height = decoder.PixelHeight();
        }

        auto pixel_data = decoder.GetPixelDataAsync(BitmapPixelFormat::Rgba8, 
                                                      BitmapAlphaMode::Premultiplied,
                                                      transform,
                                                      ExifOrientationMode::IgnoreExifOrientation,
                                                      ColorManagementMode::ColorManageToSRgb).get();
        pixel_data_detached = pixel_data.DetachPixelData();
        data = (uint8_t*) pixel_data_detached.data();

        current_track.album_art = data;
        current_track.album_art_width = width;
        current_track.album_art_height = height;
    }


    track_info_register_track_change(player, current_track);
    
    track_info_print_players();

}

static void handle_media_playback_info_change(GlobalSystemMediaTransportControlsSession session, PlaybackInfoChangedEventArgs const& args) {
    
    winrt::hstring t = session.SourceAppUserModelId();
    std::string player_t = winrt::to_string(t);
    const char* player = player_t.c_str();

    auto info = session.GetPlaybackInfo();
    if (info == nullptr) return;

    auto status = info.PlaybackStatus();
    bool playing = status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

    track_info_register_state_change(player, playing);
    
    track_info_print_players();
}

void update_players_registration() {
    vector<string> players_seen;
    auto sessions = session_manager.GetSessions();
    winrt::hstring AUMI;

    for (auto& session : sessions) {
        AUMI = session.SourceAppUserModelId();
        std::string s = winrt::to_string(AUMI);
        
        players_seen.push_back(s);
        if (find(registered_players.begin(), registered_players.end(), s) == registered_players.end()) { // not found
            session.MediaPropertiesChanged(handle_media_property_change);
            session.PlaybackInfoChanged(handle_media_playback_info_change);

            registered_players.push_back(s);
            track_info_register_player(s.c_str(), s.c_str());

            handle_media_property_change(session, NULL);
            handle_media_playback_info_change(session, NULL);
        }
    }
    sort(registered_players.begin(), registered_players.end());
    sort(players_seen.begin(), players_seen.end());
    vector<string> players_not_seen;
    set_difference(registered_players.begin(), registered_players.end(), players_seen.begin(), players_seen.end(), inserter(players_not_seen, players_not_seen.end()));

    for (string player_name: players_not_seen) {
        track_info_unregister_player(player_name.c_str());
        registered_players.erase(std::remove(registered_players.begin(), registered_players.end(), player_name), registered_players.end());
    }
}

extern "C" void player_info_init() {
    log_info("Initialising");
    track_info_init();

    try {
        session_manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    } catch (...) {
        log_error("an error occured while getting the GlobalSystemMediaTransportControlsSessionManager");
        return;
    }
    session_manager.SessionsChanged(handle_session_change);

    update_players_registration();
}

extern "C" int player_info_process() {
    return 0;
}
