oscogram: oscogram.c Makefile
	gcc -std=c99 oscogram.c -lGL -lGLEW -lglfw -lGLU -lm -g -o oscogram

clean:
	rm -f oscogram
	rm -f *~

