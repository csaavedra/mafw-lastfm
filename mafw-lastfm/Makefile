mafw-lastfm: mafw-lastfm.o mafw-lastfm-scrobbler.o
	gcc -o mafw-lastfm mafw-lastfm.o mafw-lastfm-scrobbler.o `pkg-config --libs mafw mafw-shared libsoup-2.4` -Wall -ggdb -O0

mafw-lastfm.o: mafw-lastfm.c
	gcc -c mafw-lastfm.c `pkg-config --cflags mafw mafw-shared` -Wall -ggdb -O0

mafw-lastfm-scrobbler.o: mafw-lastfm-scrobbler.c mafw-lastfm-scrobbler.h
	gcc -c mafw-lastfm-scrobbler.c `pkg-config --cflags libsoup-2.4` -Wall -ggdb -O0

clean:
	rm -f *.o mafw-lastfm

