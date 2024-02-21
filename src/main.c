#include "libavutil/frame.h"
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

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


bool stream_audio(const char *audio_url)
{
    int err;
    const char *err_msg;
    bool success = false;


    // Stream Audio
    AVFormatContext *format_ctx = NULL;
    err = avformat_open_input(&format_ctx, audio_url, NULL, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't open audio file %s: %s\n", audio_url, av_err2str(err));
        goto cleanup_av_context;
    }

    err = avformat_find_stream_info(format_ctx, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't find stream info from audio file %s: %s\n", audio_url, av_err2str(err));
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
        goto cleanup_av_context;
    }

    const AVCodec *audio_codec = avcodec_find_decoder(audio->codecpar->codec_id);
    if (audio_codec == NULL) {
        fprintf(stderr, "Unable to find audio codec: %s\n", av_err2str(err));
        goto cleanup_av_context;
    }

    AVCodecContext *audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    err = avcodec_open2(audio_codec_ctx, audio_codec, NULL);
    if (err < 0) {
        fprintf(stderr, "Couldn't find open avcodec: %s\n", av_err2str(err));
        goto cleanup_av_context;
    }


    // SDL Audio
    SDL_AudioSpec desired_spec = {
        .freq = audio_codec_ctx->sample_rate,
        .format = AUDIO_S16SYS,
        .channels = audio_codec_ctx->ch_layout.nb_channels,
        .silence = 0,
        .samples = 1024,
        .padding = 0,
        .size = 0,
        .callback = NULL,
        .userdata = audio_codec_ctx,
    };
    SDL_AudioSpec obtained_spec;

    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, 0);
    if (audio_dev == 0) {
        fprintf(stderr, "Unable to open audio device on thread: %s\n", SDL_GetError());
        goto cleanup_sdl_audio;
    }

    SDL_AudioStream *audio_stream;
    audio_stream = SDL_NewAudioStream(AUDIO_S16MSB,1, 0, 0, 0, 0 );


    // Stream Loop
    AVPacket packet;
    AVFrame *frame = malloc(sizeof(AVFrame));
    while (true) {
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
        while (err == 0) {
            err = avcodec_receive_frame(audio_codec_ctx, frame);
            if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
                break;
            } else if (err < 0) {
                fprintf(stderr, "Failed to receive frame: %s\n", av_err2str(err));
                goto cleanup_av_audio;
            }


            // ENCODE frame
            err = SDL_QueueAudio(audio_dev, frame->data, 8);
            if (err < 0) {
                err_msg = SDL_GetError();
                fprintf(stderr, "SDL Queue Audio error: %s\n", err_msg);
                goto cleanup_av_audio;
            }
            printf("Queuing Audio Frame: duration=%lld\n", frame->duration);

            av_frame_unref(frame);
        }
        if (err != AVERROR(EAGAIN) && err != AVERROR_EOF) {
            fprintf(stderr, "Error sneding audio packet: %s\n", av_err2str(err));
        }
        av_packet_unref(&packet);
        

    }


    success = true;


    cleanup_av_audio:
    cleanup_av_codec_context:
        avcodec_free_context(&audio_codec_ctx);
    cleanup_av_context:
        avformat_free_context(format_ctx);
    cleanup_sdl_audio:
        SDL_CloseAudioDevice(audio_dev);

    return success;
}

char *audio_url;

void *stream_thread(void *vargp)
{
    printf("Stream Thread\n");

    stream_audio(audio_url);

    return  NULL;
}

int main(int argc, char *argv[])
{
    int exit_code = 0;

    if (argc < 2) {
        /* audio_url = "https://coderadio-admin-v2.freecodecamp.org/listen/coderadio/radio.mp3"; */
        audio_url = "radio.mp3";
        /* return usage(stderr, 1); */
    } else {
        audio_url = argv[1];
    }

    printf("🎸 🥁 🎹 -- CodeRadio --\n");

    /* pthread_t stream_thread_id; */
    /* pthread_create(&stream_thread_id, NULL, stream_thread, NULL); */

    printf("Main Thread\n");
    // SDL Setup

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

    stream_audio(audio_url);


    SDL_Event e;
    /* bool quit = false; */

    /* while (!quit) { */
    while (true) {
        SDL_WaitEvent(&e);
        switch (e.type) {
            case SDL_QUIT:
                goto cleanup_sdl_window;
                break;
        }

    }

    printf("Finished playing audio\n");

    cleanup_sdl_window:
        SDL_DestroyWindow(window);
    cleanup_sdl_quit:
        SDL_Quit();

    return exit_code;
}

