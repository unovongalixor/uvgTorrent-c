FROM ubuntu:18.04

RUN apt update
RUN apt install -y valgrind build-essential cmocka-doc libcmocka-dev libcmocka0 libcurl4-openssl-dev gcovr lcov
# libcurl4-gnutls-dev will cause Valgrind to show a memory leak - use openssl

COPY . /app
WORKDIR /app
CMD make clean all tests && ./bin/uvgTorrent --magnet_uri="magnet:?xt=urn:btih:08ada5a7a6183aae1e09d831df6748d566095a10&dn=Sintel&tr=udp%3A%2F%2Fexplodie.org%3A6969&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969&tr=udp%3A%2F%2Ftracker.empire-js.us%3A1337&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%2F%2Ftracker.opentrackr.org%3A1337" --path="/tmp"
