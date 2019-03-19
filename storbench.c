#include <time.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define MAX_LABEL_TIME	3
#define MAX_LABEL_THRU	5
#define MAX_LABEL_TOTAL	6

void Fclose(FILE *stream)
{
	if (fclose(stream) == EOF)
	{
		perror("fclose");
	}
}

void usage(char *proc)
{
	char *usage = "Usage: %s [-d] [-b nbytes] [-f ofile]\n"
				  "Write random bytes to a file and benchmark the performance\n"
				  "Flags\n"
				  "  -b nbytes\n"
				  "\tSpecifies the number of bytes to read and write from ofile\n"
				  "  -f ofile\n"
				  "\tSpecifies the name of the file to base the benchmark on\n"
				  "  -d\n"
				  "\tDelete file ofile on completion\n";
	fprintf(stderr, usage, proc);
}

int scale_iter(const uint64_t in_time, int scale_factors[], int i)
{
	if (in_time >= scale_factors[i]) { return i; }
	scale_iter(in_time, scale_factors, ++i);
}
	
void scale_time(uint64_t in_time, float *out_scaled, char *out_label)
{
	int scale_factors[3] = {
		1000000,	/* microseconds per sec */
		1000,		/* milliseconds per sec */
		1			/* seconds per sec */
	};
	char *labels[3] = { "s", "ms", "us" };
	int r = scale_iter(in_time, scale_factors, 0);
	*out_scaled =  (float)in_time / scale_factors[r];
	strncpy(out_label, labels[r], 3);
}

void scale_throughput(uint64_t in_rate, float *out_scaled, char *out_label)
{
	int scale_factors[4] = {
		1000000000,		/* bits per gigabit */
		1000000,		/* bits per megabit */
		1000,			/* bits per kilobits */
		1				/* bits per bit*/
	};
	char *labels[4] = { "Gbps", "Mbps", "Kbps", "bps" };
	int r = scale_iter(in_rate, scale_factors, 0);
	*out_scaled =  (float)in_rate / scale_factors[r];
	strncpy(out_label, labels[r], 5);
}

void scale_bytes(uint64_t in_bytes, float *out_scaled, char *out_label)
{
	int scale_factors[4] = {
		1073741824,		/* bytes per gigabyte */
		1048576,		/* bytes per megabyte */
		1024,			/* bytes per kilobyte */
		1				/* bytes per byte*/
	};
	char *labels[4] = { "GB", "MB", "KB", "bytes" };
	int r = scale_iter(in_bytes, scale_factors, 0);
	*out_scaled =  (float)in_bytes / scale_factors[r];
	strncpy(out_label, labels[r], 6);
}

void progress_init(char *label)
{
	printf("%s: 0%%\r", label);
	fflush(stdout);
}

void progress_update(char *label, size_t total, size_t delta)
{
	static size_t bytes_accum = 0;
	bytes_accum += delta;
	double progress = ((double)bytes_accum / total) * 100;
	printf("%s: %.0f%%\r", label, progress);
	fflush(stdout);
}

void progress_term(char *label)
{
	printf("%s: 100%%\n", label);
}

int main(int argc, char * argv[])
{
	int opt;

	size_t bytes_total = 0;
	char file_inout[PATH_MAX] = {0};
	enum { FILE_KEEP_MODE, FILE_DELETE_MODE } mode = FILE_KEEP_MODE;
	while ((opt = getopt(argc, argv, ":df:b:")) != -1)
	{
		switch (opt)
		{
		case 'd':
			mode = FILE_DELETE_MODE;
			break;
		case 'f':
			snprintf(file_inout, PATH_MAX, "%s", optarg);
			break;
		case 'b':
			bytes_total = atol(optarg);
			break;
		case ':':
			fprintf(stderr, "Option %c needs a value\n", optopt);
			usage(argv[0]);
			exit(EXIT_FAILURE);
		case '?':
			fprintf(stderr, "Unrecognized option %c\n", optopt);
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	if (!file_inout[0])
	{
		fprintf(stderr, "ofile parameter is required\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if (bytes_total == 0)
	{
		fprintf(stderr, "nbytes must be a positive integer\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if (optind < argc)
	{
		fprintf(stderr, "Invalid arguments: ");
		while (optind < argc)
		{
			fprintf(stderr, "%s ", argv[optind++]);
		}
		fprintf(stderr, "\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	FILE *fd_inout;
	if ((fd_inout = fopen(file_inout, "w+b")) == NULL)
	{
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	/* Get the current heap size */
	/* Stay well below limit so we don't hog memory */
	struct rlimit data_limit;
	if (getrlimit(RLIMIT_DATA, &data_limit) == -1)
	{
		perror("getrlimit");
		exit(EXIT_FAILURE);
	}
	size_t chunk_size = (size_t)(data_limit.rlim_cur / 2);
	
	char *buf, *ptr;
	struct timespec
		read_test_start, read_test_end,
		write_test_start, write_test_end;
	size_t bytes_remaining = bytes_total, io_bytes;
	progress_init("writing");
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
		progress_term("writing");
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
			if ((io_bytes = fwrite(buf, sizeof(char), chunk_size, fd_inout)) != chunk_size)
			{
				perror("fwrite");
				Fclose(fd_inout);
				free(buf);
				exit(EXIT_FAILURE);
			}
			bytes_remaining -= chunk_size;
			ptr = buf;
			progress_update("writing", bytes_total, io_bytes);
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
		progress_term("writing");
	}
	
	/* Bench read throughput */
	rewind(fd_inout);
	progress_init("reading");
	clock_gettime(CLOCK_REALTIME, &read_test_start);
	while ((io_bytes = fread(buf, sizeof(char), chunk_size, fd_inout)), !feof(fd_inout) && !ferror(fd_inout))
	{
		progress_update("reading", bytes_total, io_bytes);
	}
	clock_gettime(CLOCK_REALTIME, &read_test_end);
	progress_term("reading");
	if (ferror(fd_inout))
	{
		perror("fread");
		Fclose(fd_inout);
		free(buf);
		exit(EXIT_FAILURE);
	}
	
	free(buf);
	Fclose(fd_inout);
	if (mode == FILE_DELETE_MODE)
	{
		if (unlink(file_inout) == -1)
		{
			perror("unlink");
			exit(EXIT_FAILURE);
		}
	}
	
	/* Calculate raw values */
	size_t bits = bytes_total * 8;
	uint64_t delta_write_us = (write_test_end.tv_sec - write_test_start.tv_sec) * 1000000 + (write_test_end.tv_nsec - write_test_start.tv_nsec) / 1000;
	uint64_t delta_read_us = (read_test_end.tv_sec - read_test_start.tv_sec) * 1000000 + (read_test_end.tv_nsec - read_test_start.tv_nsec) / 1000;
	float bits_write_s = bits / (float)((double)delta_write_us / (double)1000000);
	float bits_read_s = bits / (float)((double)delta_read_us / (double)1000000);
	
	/* Write results */
	
	/* Scale time readout */
	float write_time_scaled;
	char write_time_label[MAX_LABEL_TIME];
	scale_time(delta_write_us, &write_time_scaled, write_time_label);
	
	/* Scale throughput readout */
	float write_speed_scaled;
	char write_speed_label[MAX_LABEL_THRU];
	scale_throughput(bits_write_s, &write_speed_scaled, write_speed_label);
	
	/* Read results */
	
	/* Scale time readout */
	float read_time_scaled;
	char read_time_label[MAX_LABEL_TIME];
	scale_time(delta_read_us, &read_time_scaled, read_time_label);
	
	/* Scale throughput readout */
	float read_speed_scaled;
	char read_speed_label[MAX_LABEL_THRU];
	scale_throughput(bits_read_s, &read_speed_scaled, read_speed_label);
	
	/* Scale total bytes written */
	float total_bytes_scaled;
	char total_bytes_label[MAX_LABEL_TOTAL];
	scale_bytes(bytes_total, &total_bytes_scaled, total_bytes_label);
	
	/* Format nicely and print results */
	char read_time_fmt[15], write_time_fmt[15],
		 read_thru_fmt[15], write_thru_fmt[15];
	printf("Completed %.2f %s benchmark on %s\n", total_bytes_scaled, total_bytes_label, file_inout);
	snprintf(read_time_fmt, 15, "%.2f %s", read_time_scaled, read_time_label);
	snprintf(write_time_fmt, 15, "%.2f %s", write_time_scaled, write_time_label);
	snprintf(read_thru_fmt, 15, "%.2f %s", read_speed_scaled, read_speed_label);
	snprintf(write_thru_fmt, 15, "%.2f %s", write_speed_scaled, write_speed_label);
	printf("%-15s %-10s %-10s\n", "               ", "read", "write");
	printf("%-15s %-10s %-10s\n", "elapsed time", read_time_fmt, write_time_fmt);
	printf("%-15s %-10s %-10s\n", "throughput", read_thru_fmt, write_thru_fmt);
	
	exit(EXIT_SUCCESS);
}