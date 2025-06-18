#ifndef DECOMPRESS_H
#define DECOMPRESS_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the H.264 encoder in lossless mode.
int init_encoder_lossless(AVCodecContext* decCtx, const char* outFilename,
                          AVCodecContext** encCtx, AVFormatContext** outFmtCtx, AVStream** outStream);

// High-level function to "decompress" a video by re-encoding in lossless mode.
int decompress_video_mp4(const char* input_filename, const char* output_filename);

#ifdef __cplusplus
}
#endif

#endif // DECOMPRESS_H
