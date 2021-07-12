#define UNICODE

#pragma comment(lib, "windowsapp")

#include <iostream>

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

static void handle_session_change(GlobalSystemMediaTransportControlsSessionManager session_manager, SessionsChangedEventArgs const& args) {
    // This does not seems to be triggered
    cout << "HERE §§§" << endl;

    __debugbreak();
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
        data = pixel_data.DetachPixelData().data();

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

void register_players() {
    GlobalSystemMediaTransportControlsSessionManager session_manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    session_manager.SessionsChanged(handle_session_change);

    auto sessions = session_manager.GetSessions();
    winrt::hstring AUMI;

    for (auto& session : sessions) {
        session.MediaPropertiesChanged(handle_media_property_change);
        session.PlaybackInfoChanged(handle_media_playback_info_change);

        AUMI = session.SourceAppUserModelId();
        std::string s = winrt::to_string(AUMI);
        
        track_info_register_player(s.c_str(), s.c_str());

        handle_media_property_change(session, NULL);
        handle_media_playback_info_change(session, NULL);
    }

}

extern "C" void player_info_init() {
    log_info("Initialising");
    track_info_init();

    register_players();
}

extern "C" int player_info_process() {
    return 0;
}
