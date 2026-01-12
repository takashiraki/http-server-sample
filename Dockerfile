FROM ubuntu:latest

RUN apt update \
    && apt install -y build-essential vim

WORKDIR /app

COPY app /app

RUN gcc -o server server.c

EXPOSE 8080

CMD ["./server"]