#include "decompress.h"
#include <cstdio>
#include <cstdlib>
#include <libavutil/opt.h>

int init_encoder_lossless(AVCodecContext* decCtx, const char* outFilename,
                          AVCodecContext** encCtx, AVFormatContext** outFmtCtx, AVStream** outStream) {
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
        fprintf(stderr, "H.264 encoder not found\n");
        return -1;
    }
    *encCtx = avcodec_alloc_context3(encoder);
    if (!*encCtx) {
        fprintf(stderr, "Could not allocate encoder context\n");
        return -1;
    }
    (*encCtx)->width  = decCtx->width;
    (*encCtx)->height = decCtx->height;
    (*encCtx)->time_base = (AVRational){1, 25};
    (*encCtx)->pix_fmt = AV_PIX_FMT_YUV420P;
    // For lossless mode, set CRF=0 (and omit bitrate).
    av_opt_set((*encCtx)->priv_data, "crf", "0", 0);
    av_opt_set((*encCtx)->priv_data, "preset", "slow", 0);
    if (avcodec_open2(*encCtx, encoder, nullptr) < 0) {
        fprintf(stderr, "Could not open encoder in lossless mode\n");
        return -1;
    }
    av_opt_set((*encCtx)->priv_data, "profile", "baseline", 0);
    if (avformat_alloc_output_context2(outFmtCtx, nullptr, nullptr, outFilename) < 0) {
        fprintf(stderr, "Could not create output context\n");
        return -1;
    }
    *outStream = avformat_new_stream(*outFmtCtx, nullptr);
    if (!*outStream) {
        fprintf(stderr, "Failed allocating output stream\n");
        return -1;
    }
    int ret = avcodec_parameters_from_context((*outStream)->codecpar, *encCtx);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy lossless encoder parameters to output stream\n");
        return ret;
    }
    (*outStream)->time_base = (*encCtx)->time_base;
    if (!((*outFmtCtx)->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&((*outFmtCtx)->pb), outFilename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'\n", outFilename);
            return ret;
        }
    }
    ret = avformat_write_header(*outFmtCtx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Error writing header to output file (lossless)\n");
        return ret;
    }
    return 0;
}

int decode_video(const char* input_filename, const char* output_filename) {
    int videoStreamIndex = -1;
    AVFormatContext* inFmtCtx = nullptr;
    AVFormatContext* outFmtCtx = nullptr;
    AVCodecContext* decCtx = nullptr;
    AVCodecContext* encCtx = nullptr;
    AVStream* outStream = nullptr;
    int ret = 0;

    ret = open_input(input_filename, &inFmtCtx, &videoStreamIndex);
    if (ret < 0) goto end;
    decCtx = init_decoder(inFmtCtx, videoStreamIndex);
    if (!decCtx) { ret = -1; goto end; }
    ret = init_encoder_lossless(decCtx, output_filename, &encCtx, &outFmtCtx, &outStream);
    if (ret < 0) goto end;
    ret = process_frames(inFmtCtx, videoStreamIndex, decCtx, encCtx, outFmtCtx, outStream);

end:
    if (decCtx) avcodec_free_context(&decCtx);
    if (encCtx) avcodec_free_context(&encCtx);
    if (inFmtCtx) avformat_close_input(&inFmtCtx);
    if (outFmtCtx) {
        if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&outFmtCtx->pb);
        avformat_free_context(outFmtCtx);
    }
    return ret;
}
