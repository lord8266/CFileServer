# File Server using bsd kevents

Uses [kevents](https://www.freebsd.org/cgi/man.cgi?query=kevent) to handle multiple connections on a single thread.


## Compiling
Will compile on bsd systems only.
```
make
```

## Usage

```
./server # defaults to cwd, port 8080, 2000 r/w bufsize
./server <directory>
./server <directory> <port>
./server <directory> <port> <bufsize>
```
## Examples

```
./server 
./server / 
./server / 8080
./server / 8080 5000
```


