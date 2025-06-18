#include "compress.h"
#include <cstdio>
#include <cstdlib>
#include <libavutil/opt.h>

int init_encoder(AVCodecContext *decCtx, const char *outFilename,
                 AVCodecContext **encCtx, AVFormatContext **outFmtCtx, AVStream **outStream)
{
    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder)
    {
        fprintf(stderr, "H.264 encoder not found\n");
        return -1;
    }
    *encCtx = avcodec_alloc_context3(encoder);
    if (!*encCtx)
    {
        fprintf(stderr, "Could not allocate encoder context\n");
        return -1;
    }
    // Set properties based on the decoder context.
    (*encCtx)->width = decCtx->width;
    (*encCtx)->height = decCtx->height;
    (*encCtx)->time_base = (AVRational){1, 25};
    (*encCtx)->pix_fmt = AV_PIX_FMT_YUV420P;
    (*encCtx)->bit_rate = 1000000; // Example bitrate (1 Mbps)

    // Set encoder options:
    // Force a slower preset for quality (you can change "slow" as needed)
    av_opt_set((*encCtx)->priv_data, "preset", "slow", 0);
    // Set the baseline profile (ensures simpler coding tools and often inline SPS/PPS)
    av_opt_set((*encCtx)->priv_data, "profile", "baseline", 0);
    // Force a fixed keyframe interval:
    av_opt_set((*encCtx)->priv_data, "keyint", "25", 0);
    av_opt_set((*encCtx)->priv_data, "min-keyint", "25", 0);
    // Disable scene cut detection to force regular keyframes
    av_opt_set((*encCtx)->priv_data, "scenecut", "0", 0);

    if (avcodec_open2(*encCtx, encoder, nullptr) < 0)
    {
        fprintf(stderr, "Could not open encoder\n");
        return -1;
    }
    fprintf(stderr, "Encoder opened with profile: %s\n", "baseline");

    // Create the output format context for MP4.
    if (avformat_alloc_output_context2(outFmtCtx, nullptr, nullptr, outFilename) < 0)
    {
        fprintf(stderr, "Could not create output context\n");
        return -1;
    }
    *outStream = avformat_new_stream(*outFmtCtx, nullptr);
    if (!*outStream)
    {
        fprintf(stderr, "Failed allocating output stream\n");
        return -1;
    }
    int ret = avcodec_parameters_from_context((*outStream)->codecpar, *encCtx);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to copy encoder parameters to output stream\n");
        return ret;
    }
    (*outStream)->time_base = (*encCtx)->time_base;
    if (!((*outFmtCtx)->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&((*outFmtCtx)->pb), outFilename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Could not open output file '%s'\n", outFilename);
            return ret;
        }
    }
    ret = avformat_write_header(*outFmtCtx, nullptr);
    if (ret < 0)
    {
        fprintf(stderr, "Error writing header to output file\n");
        return ret;
    }
    return 0;
}

int encode_video(const char *input_filename, const char *output_filename)
{
    int videoStreamIndex = -1;
    AVFormatContext *inFmtCtx = nullptr;
    AVFormatContext *outFmtCtx = nullptr;
    AVCodecContext *decCtx = nullptr;
    AVCodecContext *encCtx = nullptr;
    AVStream *outStream = nullptr;
    int ret = 0;

    ret = open_input(input_filename, &inFmtCtx, &videoStreamIndex);
    if (ret < 0)
        goto end;
    decCtx = init_decoder(inFmtCtx, videoStreamIndex);
    if (!decCtx)
    {
        ret = -1;
        goto end;
    }
    ret = init_encoder(decCtx, output_filename, &encCtx, &outFmtCtx, &outStream);
    if (ret < 0)
        goto end;
    ret = process_frames(inFmtCtx, videoStreamIndex, decCtx, encCtx, outFmtCtx, outStream);

end:
    if (decCtx)
        avcodec_free_context(&decCtx);
    if (encCtx)
        avcodec_free_context(&encCtx);
    if (inFmtCtx)
        avformat_close_input(&inFmtCtx);
    if (outFmtCtx)
    {
        if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&outFmtCtx->pb);
        avformat_free_context(outFmtCtx);
    }
    return ret;
}
