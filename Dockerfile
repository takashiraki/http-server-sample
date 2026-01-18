FROM ubuntu:latest

RUN apt update \
    && apt install -y build-essential vim

WORKDIR /app

COPY app /app

RUN gcc -o http-server http-server.c

EXPOSE 8080

CMD ["./http-server"]