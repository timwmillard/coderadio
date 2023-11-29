#include "SDL2/SDL_error.h"
#include <stdio.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

int usage(FILE *fd, int code) {
    fprintf(fd, "Usage:\n");
    fprintf(fd, "   coderadio <url>\n");
    return code;
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    printf("audio callback\n");
}

int main(int argc, char *argv[]) {


    if (argc < 2) {
        return usage(stderr, 1);
    }
    char *audio_url = argv[1];

    printf("🎸 🥁 🎹 -- CodeRadio --\n");


    int err;
    const char *err_msg;

    err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (err < 0) {
        err_msg = SDL_GetError();
        fprintf(stderr, "SDL Init error: %s\n", err_msg);
        return 1;
    }

    SDL_Window * window = SDL_CreateWindow("CodeRadio", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            600, 400,
            SDL_WINDOW_RESIZABLE 
    );
    if (window == NULL) {
        fprintf(stderr, "SDL Create Window error\n");
        return 1;
    }

    /* AVFormatContext *format_ctx = avformat_alloc_context(); */
    /* if (!format_ctx) { */
    /*     fprintf(stderr, "Couldn't create AVFormatContext\n"); */
    /*     return 1; */
    /* } */
    AVFormatContext *format_ctx = NULL;
    err = avformat_open_input(&format_ctx, audio_url, NULL, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't open audio file: %s\n", audio_url);
        avformat_free_context(format_ctx);
        return 1;
    }

    err = avformat_find_stream_info(format_ctx, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't find stream info from audio file: %s\n", audio_url);
        return 1;
    }
    av_dump_format(format_ctx, 0, audio_url, 0);

    /* Find the video & audio stream */
    AVStream *video = NULL;
    AVStream *audio = NULL;
    int video_stream_idx = -1;
    int audio_stream_idx = -1;
    enum AVMediaType codec_type;
    for (int i=0; i < format_ctx->nb_streams; i++) {
        codec_type = format_ctx->streams[i]->codecpar->codec_type;
        if (codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx < 0) {
            video = format_ctx->streams[i];
            video_stream_idx = i;
        }
        if (codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx < 0) {
            audio = format_ctx->streams[i];
            audio_stream_idx = i;
        }
    }
    
    if (audio_stream_idx < 0) {
        /* Did not find a audio stream */
        fprintf(stderr, "Couldn't find audio stream[%d] in: %s\n", audio_stream_idx, audio_url);
        return 1;
    }


    const AVCodec *audio_codec = avcodec_find_decoder(audio->codecpar->codec_id);
    if (audio_codec == NULL) {
        fprintf(stderr, "Unable to find audio codec");
        return 1;
    }

    AVCodecContext *audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    err = avcodec_open2(audio_codec_ctx, audio_codec, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't find open avcodec\n");
        return 1;
    }

    SDL_AudioSpec desired_spec = {
        .freq = audio_codec_ctx->sample_rate,
        .format = AUDIO_S16SYS,
        .channels = audio_codec_ctx->ch_layout.nb_channels,
        .silence = 0,
        .samples = 1024,
        .padding = 0,
        .size = 0,
        .callback = audio_callback,
        .userdata = audio_codec_ctx,
    };
    SDL_AudioSpec obtained_spec;
    err = SDL_OpenAudio(&desired_spec, &obtained_spec);
    if (err < 0) {
        fprintf(stderr, "Unable to open audio device: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Event e;
    bool quit = false;

    while (!quit) {
        while(SDL_PollEvent(&e)) {

            switch (e.type) {
                case SDL_QUIT:
                    quit = true;
                    break;
            }

        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

