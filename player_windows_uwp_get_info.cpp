#define UNICODE

#pragma comment(lib, "windowsapp")
#pragma comment(lib, "ole32")
	
#include <stdbool.h>
#include <string.h>

#include <iostream>


#include "player_mpris_get_info.h"
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


void register_players() {
	GlobalSystemMediaTransportControlsSessionManager session_manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

	auto sessions = session_manager.GetSessions();
	GlobalSystemMediaTransportControlsSessionMediaProperties media_properties {nullptr};
	winrt::hstring AUMI;

	for (auto& session : sessions) {
		media_properties = session.TryGetMediaPropertiesAsync().get();
		if (media_properties == nullptr) continue;
			AUMI = session.SourceAppUserModelId();
			std::string s = winrt::to_string(AUMI);
		
		track_info_register_player(strdup(s.c_str()), strdup(s.c_str()));
	}

}

void update_current() {
	GlobalSystemMediaTransportControlsSessionManager session_manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

	TrackInfo current_track;
	track_info_struct_init(&current_track);
	bool playing;

	GlobalSystemMediaTransportControlsSessionMediaProperties media_properties {nullptr};

	auto session = session_manager.GetCurrentSession();
	if (session == nullptr) return;

	winrt::hstring t = session.SourceAppUserModelId();
	std::string player_t = winrt::to_string(t);
	const char* player = strdup(player_t.c_str());
	cout << "Player " << player << endl;

	media_properties = session.TryGetMediaPropertiesAsync().get();

	if (media_properties == nullptr) return;
	
	auto info = session.GetPlaybackInfo();
	std:string tmp;
	if (info == nullptr) return;

	auto status = info.PlaybackStatus();
	
	playing = status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
	
	tmp = winrt::to_string(media_properties.Title());
	current_track.title = strdup((char*) tmp.c_str());
	tmp = winrt::to_string(media_properties.Artist());
	current_track.artist = strdup((char*) tmp.c_str());
	tmp = winrt::to_string(media_properties.AlbumTitle());
	current_track.album = strdup((char*) tmp.c_str());
	current_track.update_time = time(NULL);


	auto thumbnail = media_properties.Thumbnail();
	if (thumbnail != nullptr) {
		auto stream = thumbnail.OpenReadAsync().get();
		auto decoder = BitmapDecoder::CreateAsync(stream).get();
		//auto tmp_file = Windows::Storage::CreateStreamedFileAsync("test_obs.png", stream, NULL);
		
		//auto img = decoder.GetSoftwareBitmapAsync().get();
		auto pixel_data = decoder.GetPixelDataAsync(BitmapPixelFormat::Rgba8, 
													  BitmapAlphaMode::Premultiplied,
													  BitmapTransform(),
													  ExifOrientationMode::IgnoreExifOrientation,
													  ColorManagementMode::ColorManageToSRgb).get();
		uint8_t* data = pixel_data.DetachPixelData().data();

		current_track.album_art = data;
		current_track.album_art_width = decoder.PixelWidth();
		current_track.album_art_height = decoder.PixelHeight();

		//cout << "tmp: " << tmp_folder.c_str() << cout;
		
		//auto encoder = BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), tmp_file).get();

		//auto folder = Windows::Storage::TemporaryFolder();
		
		//cout << "width: " << decoder.PixelWidth() << " height: " << decoder.PixelHeight() << endl;
		
		// wcout << L"Path: " << stream.Path();
	}


    track_info_register_track_change(player, current_track);
    track_info_register_state_change(player, playing);
	
	track_info_print_players();

}

extern "C" void mpris_init() {
    log_info("Initialising");
    track_info_init();


	register_players();
}

extern "C" int mpris_process() {
	update_current();
    return 0;
}
