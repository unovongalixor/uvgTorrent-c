# uvgTorrent -c

Messing around with the torrent protocol. Very much a work in progress. The goal is to start by implementing the UDP tracker protocol, Peer Wire protocol and finally DHT based trackerless torrent support.

The project will be structured around a declarative model - the main thread maintains state while trackers and peers running via a thread pool will check torrent state via mutex protected pointers and return completed units of work to the main thread via threadsafe queues. 

Responsibility for freeing objects received via queues will rest with the owner of the queue, who understands what type it expects to receive and how to free it.

A Dockerfile is provided as I haven't gotten around to making this portable yet.
## Quick usage

* make clean all
* make clean all tests
* make clean all valgrind

to run the binary:

./bin/uvgTorrent --magnet_uri="magnet:?xt=urn:btih:etc.etc.etc" --path="/local/download/folder"

## Docker Usage

If you aren't on x86 linux you can use docker to build test and run the client under valgrind

docker build -t uvgtorrent .
docker run --rm -ti uvgtorrent:latest

## Dependencies

- Linux OS
- cmocka
- libcurl
- valgrind
- GCC
- govr
- lcov & gcov

## Testing
Tests can be found in the test directory. after running tests you can view a coverage report by running "gcovr -v -r ." or viewing the html report in coverage/html

If you would like to see which functions are mocked look for TEST_MOCKS in Makefile

The API defined for manipulating mock objects is defined in test/mocked_functions.h, with examples found throughout the tests. In general when mocking a system call you must provide a mock object via cmockas will_return call. The mock function will retreive this value with a corresponding mock() call. 

Implementations of mocked functions can be found in test/mocked_functions.c


## More Info

https://www.libtorrent.org/udp_tracker_protocol.html
https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
https://www.bittorrent.org/beps/bep_0005.html
http://xbtt.sourceforge.net/udp_tracker_protocol.html

Some info on piece selection and choking alogirthms:

http://bittorrent.org/bittorrentecon.pdf


#### Author

Simon Bursten <smnbursten@gmail.com>
