
CC = clang

DEBUG=-g -DDEBUG

SDL_CONFIG = $(shell sdl2-config --cflags --libs)
AV_CONFIG = $(shell pkg-config --cflags --libs libavformat libavcodec libavdevice)

CFLAGS += 
LIBS += 
#-lavcodec -lavutil -lavfilter -lavdevice -lswscale -lz -lm
all: coderadio

coderadio: src/main.c
	$(CC) $(DEBUG) $(CFLAGS) $(LIBS) $(SDL_CONFIG) $(AV_CONFIG) -o coderadio src/main.c 


