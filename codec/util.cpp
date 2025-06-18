#include "util.h"
#include <cstdio>
#include <cstdlib>

int open_input(const char* filename, AVFormatContext** inFmtCtx, int* videoStreamIndex) {
    int ret = avformat_open_input(inFmtCtx, filename, nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Could not open input file '%s'\n", filename);
        return ret;
    }
    ret = avformat_find_stream_info(*inFmtCtx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Failed to retrieve input stream info\n");
        return ret;
    }
    for (unsigned i = 0; i < (*inFmtCtx)->nb_streams; i++) {
        if ((*inFmtCtx)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            *videoStreamIndex = i;
            return 0;
        }
    }
    fprintf(stderr, "No video stream found in input file\n");
    return -1;
}

AVCodecContext* init_decoder(AVFormatContext* inFmtCtx, int videoStreamIndex) {
    AVCodecParameters* codecPar = inFmtCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecPar->codec_id);
    if (!decoder) {
        fprintf(stderr, "Decoder not found\n");
        return nullptr;
    }
    AVCodecContext* decCtx = avcodec_alloc_context3(decoder);
    if (!decCtx) {
        fprintf(stderr, "Could not allocate decoder context\n");
        return nullptr;
    }
    avcodec_parameters_to_context(decCtx, codecPar);
    if (avcodec_open2(decCtx, decoder, nullptr) < 0) {
        fprintf(stderr, "Could not open decoder\n");
        avcodec_free_context(&decCtx);
        return nullptr;
    }
    return decCtx;
}

int process_frames(AVFormatContext* inFmtCtx, int videoStreamIndex,
                   AVCodecContext* decCtx, AVCodecContext* encCtx,
                   AVFormatContext* outFmtCtx, AVStream* outStream) {
    AVPacket* inPkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* frameConv = av_frame_alloc();
    if (!inPkt || !frame || !frameConv) {
        fprintf(stderr, "Could not allocate frame or packet\n");
        return -1;
    }
    // Set up conversion context if the input pixel format differs from the encoder's.
    struct SwsContext* swsCtx = nullptr;
    if (decCtx->pix_fmt != encCtx->pix_fmt) {
        swsCtx = sws_getContext(decCtx->width, decCtx->height, decCtx->pix_fmt,
                                encCtx->width, encCtx->height, encCtx->pix_fmt,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
    }
    // Allocate buffer for the converted frame.
    int numBytes = av_image_get_buffer_size(encCtx->pix_fmt, encCtx->width,
                                            encCtx->height, 32);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameConv->data, frameConv->linesize, buffer,
                         encCtx->pix_fmt, encCtx->width, encCtx->height, 32);
    frameConv->width  = encCtx->width;
    frameConv->height = encCtx->height;
    frameConv->format = encCtx->pix_fmt;

    int ret;
    while (av_read_frame(inFmtCtx, inPkt) >= 0) {
        if (inPkt->stream_index == videoStreamIndex) {
            ret = avcodec_send_packet(decCtx, inPkt);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to decoder\n");
                break;
            }
            while ((ret = avcodec_receive_frame(decCtx, frame)) >= 0) {
                if (swsCtx) {
                    sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height,
                              frameConv->data, frameConv->linesize);
                } else {
                    av_frame_ref(frameConv, frame);
                }
                frameConv->pts = av_rescale_q(frame->pts,
                                              inFmtCtx->streams[videoStreamIndex]->time_base,
                                              encCtx->time_base);
                ret = avcodec_send_frame(encCtx, frameConv);
                if (ret < 0) {
                    fprintf(stderr, "Error sending frame to encoder\n");
                    break;
                }
                AVPacket* encPkt = av_packet_alloc();
                while ((ret = avcodec_receive_packet(encCtx, encPkt)) >= 0) {
                    av_packet_rescale_ts(encPkt, encCtx->time_base, outStream->time_base);
                    encPkt->stream_index = outStream->index;
                    ret = av_interleaved_write_frame(outFmtCtx, encPkt);
                    if (ret < 0) {
                        fprintf(stderr, "Error writing packet\n");
                        break;
                    }
                    av_packet_unref(encPkt);
                }
                av_packet_free(&encPkt);
                av_frame_unref(frame);
                av_frame_unref(frameConv);
            }
        }
        av_packet_unref(inPkt);
    }
    // Flush encoder.
    avcodec_send_frame(encCtx, nullptr);
    while (avcodec_receive_packet(encCtx, inPkt) == 0) {
        av_packet_rescale_ts(inPkt, encCtx->time_base, outStream->time_base);
        inPkt->stream_index = outStream->index;
        av_interleaved_write_frame(outFmtCtx, inPkt);
        av_packet_unref(inPkt);
    }
    av_write_trailer(outFmtCtx);
    av_free(buffer);
    if (swsCtx) sws_freeContext(swsCtx);
    av_frame_free(&frame);
    av_frame_free(&frameConv);
    av_packet_free(&inPkt);
    return 0;
}
