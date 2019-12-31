# uvgTorrent -c

Messing around with the torrent protocol. Very much a work in progress. The goal is to start by implementing the UDP tracker protocol, Peer Wire protocol and finally DHT based trackerless torrent support.

The project will be structured around a declarative model - the main thread only maintains state regarding what's been downloaded and what pieces are needed and runs tracker and peer objects in a threadpool. trackers and peers will check torrent state via mutex protected pointers and return completed units of work to the main thread via threadsafe queues. 

Responsibility for freeing objects received via queues will rest with the owner of the queue, who understands what type it expects to receive and how to free it.

This will help keep the concurrency of this application organized, as the only purpose of the main thread will be to declare the current state of the torrent being downloaded so that each tracker or peer operation can be viewed and tested as a single isolated unit of work. This will also make integrating DHT support easier, as it can simply be added as another module that checks state from the main thread as needed and returns available peers via a queue.

A Dockerfile is provided as I haven't gotten around to making this portable yet.
## Quick usage

* make clean all
* make clean all tests
* make clean all valgrind

to run the binary:

./bin/uvgTorrent --magnet_uri="magnet:?xt=urn:btih:etc.etc.etc" --path="/local/download/folder"

## Dependencies

- Linux OS
- cmocka
- libcurl
- valgrind
- GCC

## Testing
Tests can be found in the test directory. 

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
