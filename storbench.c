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

void Fclose(FILE *stream)
{
	if (fclose(stream) == EOF) {
		perror("fclose");
	}
}

void usage(char *prog)
{
	printf("Write random bytes to a file and benchmark the performance\n");
	printf("storbench <filename> <size>\n");
	printf("\tfilename\tName of the output file\n");
	printf("\tsize\t\tNumber of bytes to write to the output file\n");
}

int main(int argc, char * argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Wrong number of args!\n\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	size_t bytes_total;
	if ((bytes_total = atol(argv[2])) == 0)
	{
		perror("atol");
		exit(EXIT_FAILURE);
	}
	size_t bytes_remaining = bytes_total;

	/* Get the current heap size */
	/* Stay well below limit so we don't hog memory */
	struct rlimit data_limit;
	if (getrlimit(RLIMIT_DATA, &data_limit) == -1)
	{
		perror("getrlimit");
		exit(EXIT_FAILURE);
	}
	size_t chunk_size = (size_t)(data_limit.rlim_cur / 2);

	FILE *fd_inout;
	if ((fd_inout = fopen(argv[1], "w+b")) == NULL)
	{
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	
	char *buf, *ptr;
	struct timespec
		read_test_start, read_test_end,
		write_test_start, write_test_end;
	if (bytes_remaining <= chunk_size)
	{
		if ((buf = malloc(sizeof(char) * bytes_remaining)) == NULL)
		{
			perror("malloc");
			Fclose(fd_inout);
			free(buf);
			exit(EXIT_FAILURE);
		}
		ptr = buf;
		int seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
		srand(seed);
		for (int i = 0; i < bytes_remaining; i++)
		{
			char byte = (char)((int)256 * rand() / (RAND_MAX + 1.0));
			memset(ptr++, byte, sizeof(char));
		}
		clock_gettime(CLOCK_REALTIME, &write_test_start);
		if (fwrite(buf, sizeof(char), bytes_remaining, fd_inout) != bytes_remaining)
		{
			perror("fwrite");
			Fclose(fd_inout);
			free(buf);
			exit(EXIT_FAILURE);
		}
		if (fsync(fileno(fd_inout)) == -1)
		{
			perror("fsync");
			Fclose(fd_inout);
			free(buf);
			exit(EXIT_FAILURE);
		}
		clock_gettime(CLOCK_REALTIME, &write_test_end);
	}
	else
	{
		if ((buf = malloc(sizeof(char) * chunk_size)) == NULL)
		{
			perror("malloc");
			Fclose(fd_inout);
			free(buf);
			exit(EXIT_FAILURE);
		}
		ptr = buf;
		clock_gettime(CLOCK_REALTIME, &write_test_start);
		while (bytes_remaining >= chunk_size)
		{
			int seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
			srand(seed);
			for (int i = 0; i < chunk_size; i++)
			{
				char byte = (char)((int)256 * rand() / (RAND_MAX + 1.0));
				memset(ptr++, byte, sizeof(char));
			}
			if (fwrite(buf, sizeof(char), chunk_size, fd_inout) != chunk_size)
			{
				perror("fwrite");
				Fclose(fd_inout);
				free(buf);
				exit(EXIT_FAILURE);
			}
			bytes_remaining -= chunk_size;
			ptr = buf;
		}
		if (bytes_remaining > 0)
		{
			if (fwrite(buf, sizeof(char), bytes_remaining, fd_inout) != bytes_remaining)
			{
				perror("fwrite");
				Fclose(fd_inout);
				free(buf);
				exit(EXIT_FAILURE);
			}
		}
		if (fsync(fileno(fd_inout)) == -1)
		{
			perror("fsync");
			Fclose(fd_inout);
			free(buf);
			exit(EXIT_FAILURE);
		}
		clock_gettime(CLOCK_REALTIME, &write_test_end);
	}
	
	/* Bench read throughput */
	rewind(fd_inout);
	clock_gettime(CLOCK_REALTIME, &read_test_start);
	while (fread(buf, sizeof(char), chunk_size, fd_inout), !feof(fd_inout) && !ferror(fd_inout));
	clock_gettime(CLOCK_REALTIME, &read_test_end);
	if (ferror(fd_inout))
	{
		perror("fread");
		Fclose(fd_inout);
		free(buf);
		exit(EXIT_FAILURE);
	}
	
	free(buf);
	Fclose(fd_inout);
	
	size_t bits = bytes_total * 8;
	uint64_t delta_write_us = (write_test_end.tv_sec - write_test_start.tv_sec) * 1000000 + (write_test_end.tv_nsec - write_test_start.tv_nsec) / 1000;
	uint64_t delta_read_us = (read_test_end.tv_sec - read_test_start.tv_sec) * 1000000 + (read_test_end.tv_nsec - read_test_start.tv_nsec) / 1000;
	float bits_write_s = bits / (float)((double)delta_write_us / (double)1000000);
	float bits_read_s = bits / (float)((double)delta_read_us / (double)1000000);
	
	printf("%-15s %-15d\n", "bytes written", bytes_total);
	
	/* Scale seconds */
	if (delta_write_us >= SECONDS)
	{
		printf("%-15s ~%.2f s\n", "elapsed time", (float)delta_write_us / SECONDS);
	}
	else if (delta_write_us >= MILLISECONDS)
	{
		printf("%-15s ~%.2f ms\n", "elapsed time", (float)delta_write_us / MILLISECONDS);
	}
	else /* Microseconds */
	{
		printf("%-15s ~%.2f us\n", "elapsed time", (float)delta_write_us);
	}
	
	/* Scale throughput readout */
	if (bits_write_s >= GIGABITS)
	{
		printf("%-15s ~%.2f Gbps\n", "throughput", bits_write_s / GIGABITS);
	}
	else if (bits_write_s >= MEGABITS)
	{
		printf("%-15s ~%.2f Mbps\n", "throughput", bits_write_s / MEGABITS);
	}
	else if (bits_write_s >= KILOBITS)
	{
		printf("%-15s ~%.0f Kbps\n", "throughput", bits_write_s / KILOBITS);
	}
	else /* BITS */
	{
		printf("%-15s ~%.0f bps\n", "throughput", bits_write_s);
	}
	
	/* Scale seconds */
	if (delta_read_us >= SECONDS)
	{
		printf("%-15s ~%.2f s\n", "elapsed time", (float)delta_read_us / SECONDS);
	}
	else if (delta_read_us >= MILLISECONDS)
	{
		printf("%-15s ~%.2f ms\n", "elapsed time", (float)delta_read_us / MILLISECONDS);
	}
	else /* Microseconds */
	{
		printf("%-15s ~%.2f us\n", "elapsed time", (float)delta_read_us);
	}
	
	/* Scale throughput readout */
	if (bits_read_s >= GIGABITS)
	{
		printf("%-15s ~%.2f Gbps\n", "throughput", bits_read_s / GIGABITS);
	}
	else if (bits_read_s >= MEGABITS)
	{
		printf("%-15s ~%.2f Mbps\n", "throughput", bits_read_s / MEGABITS);
	}
	else if (bits_read_s >= KILOBITS)
	{
		printf("%-15s ~%.0f Kbps\n", "throughput", bits_read_s / KILOBITS);
	}
	else /* BITS */
	{
		printf("%-15s ~%.0f bps\n", "throughput", bits_read_s);
	}
	exit(EXIT_SUCCESS);
}