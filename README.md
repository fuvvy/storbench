# Storbench
Storbench is a simple, sequential read & write storage benchmarking tool written in C.
### Build
```sh
$ make all
```
### Usage
```sh
storbench [-d] [-b nbytes] [-f ofile]
Write random bytes to a file and benchmark the performance
Flags
  -b nbytes
        Specifies the number of bytes to read and write from ofile
  -f ofile
        Specifies the name of the file to base the benchmark on
  -d
        Delete file ofile on completion
```
### Example run
```sh
$ storbench -b 1073741824 -f ~/1GB -d
writing: 100%
reading: 100%
Completed 1.00 GB benchmark on /home/fuvvy/1GB
                read       write
elapsed time    635.18 ms  68.14 s
throughput      13.52 Gbps 126.06 Mbps
```
