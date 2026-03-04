#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include "image_loader.h"
#include "utils.h"
#define LOG_PREFIX "[obs_media_info] "
#include "logging.h"

void free_decoded_image(struct DecodedImage* img)
{
    if (img == NULL) return;
    efree(img->data);
    efree(img);
}

struct DecodedImage* load_image_ffmpeg(const char* url)
{
    AVFormatContext* fmt_ctx = NULL;
    AVCodecContext* dec_ctx = NULL;
    const AVCodec* decoder = NULL;
    AVFrame* frame = NULL;
    AVPacket* pkt = NULL;
    struct SwsContext* sws_ctx = NULL;
    struct DecodedImage* image = NULL;

    int stream_idx;
    int ret;

    if (url == NULL) return NULL;

    if ((ret = avformat_open_input(&fmt_ctx, url, NULL, NULL)) < 0) {
        log_warning("ffmpeg: could not open input %s\n", url);
        goto cleanup;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        log_warning("ffmpeg: could not find stream info\n");
        goto cleanup;
    }

    stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (stream_idx < 0) {
        log_warning("ffmpeg: no video stream found\n");
        goto cleanup;
    }

    AVStream* st = fmt_ctx->streams[stream_idx];

    decoder = avcodec_find_decoder(st->codecpar->codec_id);
    if (decoder == NULL) {
        log_warning("ffmpeg: decoder not found\n");
        goto cleanup;
    }

    dec_ctx = avcodec_alloc_context3(decoder);
    if (dec_ctx == NULL) {
        log_warning("ffmpeg: alloc codec context failed\n");
        goto cleanup;
    }

    if ((ret = avcodec_parameters_to_context(dec_ctx, st->codecpar)) < 0) {
        log_warning("ffmpeg: parameters_to_context failed\n");
        goto cleanup;
    }

    if ((ret = avcodec_open2(dec_ctx, decoder, NULL)) < 0) {
        log_warning("ffmpeg: avcodec_open2 failed\n");
        goto cleanup;
    }

    frame = av_frame_alloc();
    pkt = av_packet_alloc();

    if (frame == NULL || pkt == NULL) {
        log_warning("ffmpeg: alloc frame/packet failed\n");
        goto cleanup;
    }

    while (av_read_frame(fmt_ctx, pkt) >= 0) {

        if (pkt->stream_index != stream_idx) {
            av_packet_unref(pkt);
            continue;
        }

        if (avcodec_send_packet(dec_ctx, pkt) < 0) {
            av_packet_unref(pkt);
            break;
        }

        av_packet_unref(pkt);

        if (avcodec_receive_frame(dec_ctx, frame) == 0) {

            image = malloc(sizeof(struct DecodedImage));
            allocfail_exit(image);

            image->width  = frame->width;
            image->height = frame->height;

            int buffer_size = av_image_get_buffer_size(
                AV_PIX_FMT_RGBA,
                frame->width,
                frame->height,
                1
            );

            if (buffer_size < 0) {
                log_warning("ffmpeg: invalid buffer size\n");
                goto cleanup;
            }

            image->data = malloc(buffer_size);
            allocfail_exit(image->data);

            uint8_t* dest_data[4];
            int dest_linesize[4];

            if (av_image_fill_arrays(
                    dest_data,
                    dest_linesize,
                    image->data,
                    AV_PIX_FMT_RGBA,
                    frame->width,
                    frame->height,
                    1) < 0) {
                log_warning("ffmpeg: fill arrays failed\n");
                goto cleanup;
            }

            image->linesize = dest_linesize[0];

            sws_ctx = sws_getContext(
                frame->width,
                frame->height,
                dec_ctx->pix_fmt,
                frame->width,
                frame->height,
                AV_PIX_FMT_RGBA,
                SWS_BILINEAR,
                NULL, NULL, NULL
            );

            if (sws_ctx == NULL) {
                log_warning("ffmpeg: sws_getContext failed\n");
                goto cleanup;
            }

            sws_scale(
                sws_ctx,
                (const uint8_t* const*)frame->data,
                frame->linesize,
                0,
                frame->height,
                dest_data,
                dest_linesize
            );

            break;
        }
    }

cleanup:

    if (sws_ctx != NULL)
        sws_freeContext(sws_ctx);

    if (pkt != NULL)
        av_packet_free(&pkt);

    if (frame != NULL)
        av_frame_free(&frame);

    if (dec_ctx != NULL)
        avcodec_free_context(&dec_ctx);

    if (fmt_ctx != NULL)
        avformat_close_input(&fmt_ctx);

    return image;
}

