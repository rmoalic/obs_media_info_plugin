#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DecodedImage {
    uint8_t* data;
    int width;
    int height;
    int linesize;
};

void free_decoded_image(struct DecodedImage* img);
struct DecodedImage* load_image_ffmpeg(const char* url);

#ifdef __cplusplus
} //end extern "C"
#endif
#endif
