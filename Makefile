
CC = clang

SDL_CONFIG = $(shell sdl2-config --cflags --libs)


CFLAGS = 
LIBS = -lavformat 
#-lavcodec -lavutil -lavfilter -lavdevice -lswscale -lz -lm
all: coderadio

coderadio: src/main.c
	$(CC) -g $(CFLAGS) $(LIBS) $(SDL_CONFIG) -o coderadio src/main.c 


