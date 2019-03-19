storbench : storbench.o
	cc -o storbench storbench.o -lm
storbench.o : storbench.c
	cc -c storbench.c
all : storbench
clean :
	-rm storbench storbench.o
