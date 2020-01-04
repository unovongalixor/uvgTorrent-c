FROM ubuntu:18.04

RUN apt update
RUN apt install -y valgrind build-essential cmocka-doc libcmocka-dev libcmocka0 libcurl4-openssl-dev gcovr
# libcurl4-gnutls-dev will cause Valgrind to show a memory leak - use openssl

COPY . /app
WORKDIR /app
CMD make clean all tests valgrind