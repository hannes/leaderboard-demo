all: cruncher loader

cruncher: cruncher.c
	gcc -I. -O3 -o cruncher cruncher.c 

loader: loader.c
	gcc -I. -O3 -o loader loader.c 

clean:
	rm loader cruncher