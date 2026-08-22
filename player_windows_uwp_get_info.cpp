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
winrt::event_token session_manager_change_token;

struct PlayerSessionData {
    std::string app_id;
    GlobalSystemMediaTransportControlsSession::MediaPropertiesChanged_revoker media_properties_revoker;
    GlobalSystemMediaTransportControlsSession::PlaybackInfoChanged_revoker playback_info_revoker;
};
static vector<PlayerSessionData> registered_player_sessions;

static void handle_session_change(GlobalSystemMediaTransportControlsSessionManager session_manager_l, SessionsChangedEventArgs args) {
    update_players_registration();
}

static void handle_media_property_change(GlobalSystemMediaTransportControlsSession session, MediaPropertiesChangedEventArgs arg) {
    if (session == nullptr) return;
    TrackInfo current_track;
    track_info_struct_init(&current_track);

    std::string player;
    try {
        player = winrt::to_string(session.SourceAppUserModelId());
    } catch (...) {
        return;
    }
    auto info = session.GetPlaybackInfo();
    if (info == nullptr) return;

    GlobalSystemMediaTransportControlsSessionMediaProperties media_properties{ nullptr };

    try {
        media_properties = session.TryGetMediaPropertiesAsync().get();
    } catch (winrt::hresult_error const& e) {
        log_warning("Error fetching properties: 0x%08X - %s\n", static_cast<uint32_t>(e.code()), winrt::to_string(e.message()).c_str());
        return;
    }
    if (media_properties == nullptr) return;

    std::string title = winrt::to_string(media_properties.Title());
    current_track.title = const_cast<char*>(title.c_str());
    std::string artist = winrt::to_string(media_properties.Artist());
    current_track.artist = const_cast<char*>(artist.c_str());
    std::string album = winrt::to_string(media_properties.AlbumTitle());
    current_track.album = const_cast<char*>(album.c_str());

    auto thumbnail = media_properties.Thumbnail();
    com_array<uint8_t> pixel_data_detached;
    uint8_t* data = NULL;
    if (thumbnail != nullptr) {
        try {
            auto stream = thumbnail.OpenReadAsync().get();
            auto decoder = BitmapDecoder::CreateAsync(stream).get();
            auto transform = BitmapTransform();
            uint32_t width = decoder.PixelWidth();
            uint32_t height = decoder.PixelHeight();

            auto pixel_data = decoder.GetPixelDataAsync(
                BitmapPixelFormat::Rgba8,
                BitmapAlphaMode::Premultiplied,
                transform,
                ExifOrientationMode::IgnoreExifOrientation,
                ColorManagementMode::ColorManageToSRgb
            ).get();

            pixel_data_detached = pixel_data.DetachPixelData();
            data = (uint8_t*) pixel_data_detached.data();

            current_track.album_art = data;
            current_track.album_art_size = width * height * 4;
            current_track.album_art_width = width;
            current_track.album_art_height = height;
        }
        catch (...) {
            log_warning("Failed to decode thumbnail stream");
        }
    }

    track_info_register_track_change(player.c_str(), current_track);

}

static void handle_media_playback_info_change(GlobalSystemMediaTransportControlsSession session, PlaybackInfoChangedEventArgs args)
{
    if (session == nullptr) return;

    std::string player;
    try {
        player = winrt::to_string(session.SourceAppUserModelId());
    } catch (...) {
        return;
    }

    auto info = session.GetPlaybackInfo();
    if (info == nullptr) return;

    auto status = info.PlaybackStatus();
    bool playing = status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

    track_info_register_state_change(player.c_str(), playing);
}

void update_players_registration() {
    vector<string> players_seen;
    auto sessions = session_manager.GetSessions();

    for (auto session : sessions) {
        std::string AUMI = winrt::to_string(session.SourceAppUserModelId());
        players_seen.push_back(AUMI);

        auto it = std::find_if(registered_player_sessions.begin(), registered_player_sessions.end(),
            [&AUMI](const PlayerSessionData& p) { return p.app_id == AUMI; });

        if (it == registered_player_sessions.end()) {
            PlayerSessionData new_player;
            new_player.app_id = AUMI;
            new_player.media_properties_revoker = session.MediaPropertiesChanged(winrt::auto_revoke, handle_media_property_change);
            new_player.playback_info_revoker = session.PlaybackInfoChanged(winrt::auto_revoke, handle_media_playback_info_change);

            registered_player_sessions.push_back(std::move(new_player));
            track_info_register_player(AUMI.c_str(), AUMI.c_str());

            handle_media_property_change(session, NULL);
            handle_media_playback_info_change(session, NULL);
        }
    }

    auto it = registered_player_sessions.begin();
    while (it != registered_player_sessions.end()) {
        if (std::find(players_seen.begin(), players_seen.end(), it->app_id) == players_seen.end()) {
            track_info_unregister_player(it->app_id.c_str());
            it = registered_player_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

extern "C" void player_info_init() {
    log_info("Initialising");
    track_info_init();

    winrt::init_apartment();

    try {
        session_manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    } catch (...) {
        log_error("an error occurred while getting the GlobalSystemMediaTransportControlsSessionManager");
        return;
    }
    session_manager_change_token = session_manager.SessionsChanged(handle_session_change);

    update_players_registration();
}

extern "C" int player_info_process() {
    return 0;
}

extern "C" void player_info_close() {
    registered_player_sessions.clear();

    if (session_manager && session_manager_change_token) {
        session_manager.SessionsChanged(session_manager_change_token);
        session_manager_change_token = {};
    }
    session_manager = nullptr;

    winrt::uninit_apartment();
}
