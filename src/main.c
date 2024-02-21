#include "libavutil/frame.h"
#include <stdio.h>
#include <stdbool.h>

#include <SDL.h>
#include <SDL_audio.h>

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

    int exit_code = 0;

    char *audio_url;
    if (argc < 2) {
        audio_url = "https://coderadio-admin-v2.freecodecamp.org/listen/coderadio/radio.mp3";
        /* return usage(stderr, 1); */
    } else {
        audio_url = argv[1];
    }

    printf("🎸 🥁 🎹 -- CodeRadio --\n");


    int err;
    const char *err_msg;

    err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (err < 0) {
        err_msg = SDL_GetError();
        fprintf(stderr, "SDL Init error: %s\n", err_msg);
        exit_code = 1;
        goto cleanup_sdl_quit;
    }

    SDL_Window * window = SDL_CreateWindow("Code Radio", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            600, 400,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED
    );
    if (window == NULL) {
        fprintf(stderr, "SDL Create Window error\n");
        exit_code = 1;
        goto cleanup_sdl_quit;
    }

    AVFormatContext *format_ctx = NULL;
    err = avformat_open_input(&format_ctx, audio_url, NULL, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't open audio file %s: %s\n", audio_url, av_err2str(err));
        exit_code = 1;
        goto cleanup_av_context;
    }

    err = avformat_find_stream_info(format_ctx, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't find stream info from audio file %s: %s\n", audio_url, av_err2str(err));
        exit_code = 1;
        goto cleanup_av_context;
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
        exit_code = 1;
        goto cleanup_av_context;
    }


    const AVCodec *audio_codec = avcodec_find_decoder(audio->codecpar->codec_id);
    if (audio_codec == NULL) {
        fprintf(stderr, "Unable to find audio codec: %s\n", av_err2str(err));
        exit_code = 1;
        goto cleanup_av_context;
    }

    AVCodecContext *audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    err = avcodec_open2(audio_codec_ctx, audio_codec, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't find open avcodec: %s\n", av_err2str(err));
        exit_code = 1;
        goto cleanup_av_context;
    }

    SDL_AudioSpec desired_spec = {
        .freq = audio_codec_ctx->sample_rate,
        .format = AUDIO_S16SYS,
        .channels = audio_codec_ctx->ch_layout.nb_channels,
        .silence = 0,
        .samples = 1024,
        .padding = 0,
        .size = 0,
        /* .callback = audio_callback, */
        .callback = NULL,
        .userdata = audio_codec_ctx,
    };
    SDL_AudioSpec obtained_spec;

    SDL_AudioDeviceID audio_dev;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, 0);
    if (err < 0) {
        fprintf(stderr, "Unable to open audio device: %s\n", SDL_GetError());
        exit_code = 1;
        goto cleanup_av_audio;
    }

    SDL_AudioStream *audio_stream;
    audio_stream = SDL_NewAudioStream(AUDIO_S16MSB,1, 0, 0, 0, 0 );

    SDL_Event e;
    /* bool quit = false; */

    /* while (!quit) { */
    AVPacket packet;
    AVFrame *frame = malloc(sizeof(AVFrame));
    while(av_read_frame(format_ctx, &packet)>=0) {
    /* while (true) { */
        SDL_WaitEvent(&e);
        switch (e.type) {
            case SDL_QUIT:
                goto cleanup_av_audio;
                break;
        }

        err = av_read_frame(format_ctx, &packet);
        if (err == AVERROR_EOF) {
            continue;
            printf("Audio file finished\n");
            goto cleanup_av_audio;
        }
        if (err < 0) {
            fprintf(stderr, "Error reading audio frame: %s\n", av_err2str(err));
            goto cleanup_av_audio;
        }

        err = avcodec_send_packet(audio_codec_ctx, &packet);
        if (err < 0) {
            fprintf(stderr, "Failed to decode packet: %s\n", av_err2str(err));
            exit_code = 1;
            goto cleanup_av_audio;
        }

        err = avcodec_receive_frame(audio_codec_ctx, frame);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
            continue;
        } else if (err < 0) {
            fprintf(stderr, "Failed to receive frame: %s\n", av_err2str(err));
            exit_code = 1;
            goto cleanup_av_audio;
        }

        /* uint8_t **data = frame.data; */

        /* err = SDL_AudioStreamPut(audio_stream, frame.data, frame.nb_samples); */
        err = SDL_QueueAudio(audio_dev, frame->data, frame->nb_samples);
        if (err < 0) {
            err_msg = SDL_GetError();
            fprintf(stderr, "SDL Queue Audio error: %s\n", err_msg);
            exit_code = 1;
            goto cleanup_av_audio;
        }

        av_packet_unref(&packet);
        
    }

    printf("Finished playing audio\n");

    cleanup_av_audio:
        SDL_CloseAudioDevice(audio_dev);
    cleanup_av_codec_context:
        avcodec_free_context(&audio_codec_ctx);
    cleanup_av_context:
        avformat_free_context(format_ctx);
    cleanup_sdl_window:
        SDL_DestroyWindow(window);
    cleanup_sdl_quit:
        SDL_Quit();

    return exit_code;
}

