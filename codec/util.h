#ifndef UTIL_H
#define UTIL_H

extern "C" {
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
  #include <libswscale/swscale.h>
  #include <libavutil/imgutils.h>
  #include <libavutil/opt.h>
}

// Opens the input file and finds the first video stream.
int open_input(const char* filename, AVFormatContext** inFmtCtx, int* videoStreamIndex);

// Initializes the decoder context for the given video stream.
AVCodecContext* init_decoder(AVFormatContext* inFmtCtx, int videoStreamIndex);

// Processes frames: decodes from input, converts if needed, encodes, and writes to output.
int process_frames(AVFormatContext* inFmtCtx, int videoStreamIndex,
                   AVCodecContext* decCtx, AVCodecContext* encCtx,
                   AVFormatContext* outFmtCtx, AVStream* outStream);

#endif // UTIL_H
