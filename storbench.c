#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

/* Bits per X */
#define KILOBITS 1000
#define MEGABITS 1000000
#define GIGABITS 1000000000

/* Microseconds per X */
#define MILLISECONDS	1000
#define SECONDS			1000000

void usage(char *);

int main(int argc, char * argv[])
{
	FILE *fd_out;
	char *buf, *ptr, byte;
	int i, seed;
	size_t bytes_remaining, bytes_total, chunk;
	struct rlimit data_limit;
	struct timespec start, end;

	if (argc != 3)
	{
		fprintf(stderr, "Wrong number of args!\n\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if ((bytes_total = atol(argv[2])) == 0)
	{
		perror("atol");
		exit(EXIT_FAILURE);
	}
	bytes_remaining = bytes_total;

	/* Get the current heap size */
	/* Stay well below limit so we don't hog memory */
	if (getrlimit(RLIMIT_DATA, &data_limit) == -1)
	{
		perror("getrlimit");
		exit(EXIT_FAILURE);
	}
	chunk = (size_t)(data_limit.rlim_cur / 2);

	if ((fd_out = fopen(argv[1], "w")) == NULL)
	{
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	if (bytes_remaining <= chunk)
	{
		if ((buf = malloc(sizeof(char) * bytes_remaining)) == NULL)
		{
			perror("malloc");
			if (fclose(fd_out) == EOF) { perror("fclose"); }
			exit(EXIT_FAILURE);
		}
		ptr = buf;
		seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
		srand(seed);
		for (i = 0; i < bytes_remaining; i++)
		{
			byte = (char)((int)256 * rand() / (RAND_MAX + 1.0));
			memset(ptr++, byte, sizeof(char));
		}
		clock_gettime(CLOCK_REALTIME, &start);
		if (fwrite(buf, sizeof(char), bytes_remaining, fd_out) != bytes_remaining)
		{
			perror("fwrite");
			if (fclose(fd_out) == EOF) { perror("fclose"); }
			exit(EXIT_FAILURE);
		}
		if (fsync(fileno(fd_out)) == -1)
		{
			perror("fsync");
			if (fclose(fd_out) == EOF) { perror("fclose"); }
			exit(EXIT_FAILURE);
		}
		clock_gettime(CLOCK_REALTIME, &end);
	}
	else
	{
		if ((buf = malloc(sizeof(char) * chunk)) == NULL)
		{
			perror("malloc");
			if (fclose(fd_out) == EOF) { perror("fclose"); }
			exit(EXIT_FAILURE);
		}
		ptr = buf;
		clock_gettime(CLOCK_REALTIME, &start);
		while (bytes_remaining >= chunk)
		{
			seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
			srand(seed);
			for (i = 0; i < chunk; i++)
			{
				byte = (char)((int)256 * rand() / (RAND_MAX + 1.0));
				memset(ptr++, byte, sizeof(char));
			}
			if (fwrite(buf, sizeof(char), chunk, fd_out) != chunk)
			{
				perror("fwrite");
				if (fclose(fd_out) == EOF) { perror("fclose"); }
				exit(EXIT_FAILURE);
			}
			bytes_remaining -= chunk;
			ptr = buf;
		}
		if (bytes_remaining > 0)
		{
			if (fwrite(buf, sizeof(char), bytes_remaining, fd_out) != bytes_remaining)
			{
				perror("fwrite");
				if (fclose(fd_out) == EOF) { perror("fclose"); }
				exit(EXIT_FAILURE);
			}
		}
		if (fsync(fileno(fd_out)) == -1)
		{
			perror("fsync");
			if (fclose(fd_out) == EOF) { perror("fclose"); }
			exit(EXIT_FAILURE);
		}
		clock_gettime(CLOCK_REALTIME, &end);
	}
	free(buf);
	if (fclose(fd_out) == EOF) { perror("fclose"); }
	
	size_t bits = bytes_total * 8;
	uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
	float bits_s = bits / (float)((double)delta_us / (double)1000000);
	
	printf("%-15s %-15d\n", "bytes written", bytes_total);
	
	/* Scale seconds */
	if (delta_us >= SECONDS)
	{
		printf("%-15s ~%.2f s\n", "elapsed time", (float)delta_us / SECONDS);
	}
	else if (delta_us >= MILLISECONDS)
	{
		printf("%-15s ~%.2f ms\n", "elapsed time", (float)delta_us / MILLISECONDS);
	}
	else /* Microseconds */
	{
		printf("%-15s ~%.2f us\n", "elapsed time", (float)delta_us);
	}
	
	/* Scale throughput readout */
	if (bits_s >= GIGABITS)
	{
		printf("%-15s ~%.2f Gbps\n", "throughput", bits_s / GIGABITS);
	}
	else if (bits_s >= MEGABITS)
	{
		printf("%-15s ~%.2f Mbps\n", "throughput", bits_s / MEGABITS);
	}
	else if (bits_s >= KILOBITS)
	{
		printf("%-15s ~%.0f Kbps\n", "throughput", bits_s / KILOBITS);
	}
	else /* BITS */
	{
		printf("%-15s ~%.0f bps\n", "throughput", bits_s);
	}
	exit(EXIT_SUCCESS);
}

void usage(char *prog)
{
	printf("Write random bytes to a file and benchmark the performance\n");
	printf("storbench <filename> <size>\n");
	printf("\tfilename\tName of the output file\n");
	printf("\tsize\t\tNumber of bytes to write to the output file\n");
}
