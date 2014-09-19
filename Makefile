oscogram: oscogram.c Makefile
	gcc -std=c99 oscogram.c -lGL -lGLEW -lglfw -lGLU -llo -lfftw3 -lm -g -o oscogram

clean:
	rm -f oscogram
	rm -f *~

