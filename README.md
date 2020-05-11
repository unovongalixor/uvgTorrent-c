# uvgTorrent -c

Messing around with the torrent protocol. Very much a work in progress. The goal is to start by implementing the UDP tracker protocol, Peer Wire protocol and finally DHT based trackerless torrent support.

The project will be structured around a declarative model - the main thread maintains state while trackers and peers running via a thread pool will check torrent state via mutex protected pointers and return completed units of work to the main thread via threadsafe queues. 

Responsibility for freeing objects received via queues will rest with the owner of the queue, who understands what type it expects to receive and how to free it.

A Dockerfile is provided as I haven't gotten around to making this portable yet.

Also, i currently have no intention to get into the finer details of good behavior. i wouldn't recommend using this as a go to torrent client as real ones have had much more care put into behaving properly, for example requesting rare pieces first and intelligently managing uploads.


## Quick usage

* make clean all
* make clean all tests
* make clean all valgrind

to run the binary (this is a public domain example):

./bin/uvgTorrent --magnet_uri="magnet:?xt=urn:btih:08ada5a7a6183aae1e09d831df6748d566095a10&dn=Sintel&tr=udp%3A%2F%2Fexplodie.org%3A6969&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969&tr=udp%3A%2F%2Ftracker.empire-js.us%3A1337&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%2F%2Ftracker.opentrackr.org%3A1337" --path="/tmp"

if you give uvgTorrent non udp trackers at the moment you'll get weird errors.

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
- gcovr
- lcov & gcov

## Testing
Tests can be found in the test directory. after running tests you can view a coverage report by running "gcovr -v -r ." or viewing the html report in coverage/html

I've set aside the testing for now and will revisit it once the main functionality is completed.


## More Info

and this paper provides a good overview i would start here:

https://arxiv.org/pdf/1402.2187.pdf

these are various docs describing the various protocols:

https://www.libtorrent.org/udp_tracker_protocol.html
https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
https://www.libtorrent.org/extension_protocol.html
https://www.bittorrent.org/beps/bep_0005.html
http://www.bittorrent.org/beps/bep_0009.html
http://xbtt.sourceforge.net/udp_tracker_protocol.html

Some info on piece selection and choking alogirthms:

http://bittorrent.org/bittorrentecon.pdf

Some info on libtorrent configuration values for things like reconnect settings, etc

https://www.libtorrent.org/reference-Settings.html

the libtorrent manual is also an excellent resource to understand some of the finer grained algorithms for manging seeding and downloading behavior:

https://libtorrent.org/manual.html


#### Author

Simon Bursten <smnbursten@gmail.com>
