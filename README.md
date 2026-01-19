# my-http-server

A minimal HTTP server written in C.

## Overview

This is a simple HTTP server that listens on port 8080 and returns "Hello world" for all requests. Built for learning purposes to understand socket programming basics.

## Usage

### Using Docker (Recommended)

```bash
# Pull and run
docker run -p 8080:8080 manemanedraw/my-http-server:test

# Or build locally
docker build -t my-http-server .
docker run -p 8080:8080 my-http-server
```

### Building from Source

```bash
cd app
gcc -o http-server http-server.c
./http-server
```

### Test the Server

```bash
curl http://localhost:8080
# Response: Hello world
```

## License

MIT
