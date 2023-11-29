#include <stdio.h>

#include <libavformat/avformat.h>

int play_video() {

    int err;

    AVFormatContext *format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        fprintf(stderr, "Couldn't create AVFormatContext\n");
        return 1;
    }
    err = avformat_open_input(&format_ctx, "radio.mp3", NULL, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't open video file\n");
        avformat_free_context(format_ctx);
        return 1;
    }
    

    return 0;
}


