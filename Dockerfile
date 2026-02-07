FROM ubuntu:latest

RUN apt update \
    && apt install -y build-essential vim gdb git curl lsof psmisc tmux \
    && LAZYGIT_VERSION=$(curl -s "https://api.github.com/repos/jesseduffield/lazygit/releases/latest" | grep -Po '"tag_name": "v\K[^"]*') \
    && curl -Lo lazygit.tar.gz "https://github.com/jesseduffield/lazygit/releases/latest/download/lazygit_${LAZYGIT_VERSION}_Linux_x86_64.tar.gz" \
    && tar xf lazygit.tar.gz lazygit \
    && install lazygit /usr/local/bin \
    && rm lazygit.tar.gz lazygit

COPY var /var

WORKDIR /app

COPY app /app

RUN gcc -o http-server http-server.c

EXPOSE 8080

CMD ["./http-server"]