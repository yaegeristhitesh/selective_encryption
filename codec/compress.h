#ifndef COMPRESS_H
#define COMPRESS_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the H.264 encoder for lossy compression.
int init_encoder(AVCodecContext* decCtx, const char* outFilename,
                 AVCodecContext** encCtx, AVFormatContext** outFmtCtx, AVStream** outStream);

// High-level function to encode (compress) a video.
int encode_video(const char* input_filename, const char* output_filename);

#ifdef __cplusplus
}
#endif

#endif // COMPRESS_H
