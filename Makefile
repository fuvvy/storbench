storbench : storbench.o
	cc -o storbench storbench.o
storbench.o : storbench.c
	cc -c storbench.c
all : storbench
clean :
	-rm storbench storbench.o
