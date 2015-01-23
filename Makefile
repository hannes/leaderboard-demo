all: cruncher loader

cruncher: cruncher.c
	gcc -g -o cruncher cruncher.c -I.

loader: loader.c
	gcc -g -o loader loader.c -I.