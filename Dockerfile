FROM debian:bookworm
RUN dpkg --add-architecture arm64 && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu libsdl2-dev:arm64
WORKDIR /src
