# uvgTorrent -c

Messing around with the torrent protocol. Very much a work in progress. The goal is to start by implementing the UDP tracker protocol, Peer Wire protocol and finally DHT based trackerless torrent support.

Project will be structured around a thread pool distributing tasks and returning the results of jobs to various queues. for example, a queue for peers received from Trackers (and in the future from the DHT, the consumer won't care where the peer came from), a queue for metadata chunks from peers, a queue for pieces of files from peers, etc.
State will be managed via mutex protected shared memory, i.e. check the status of a tracker before deciding what kind of work to dispatch to the thread queue.

Responsibility for freeing object received via queues will rest with the owner of the queue, who understands what type it expects to receive and how to free it.

This will help keep the concurrency of this application organized, as the only purpose of the main thread will be to route tasks via the thread pool and each task can easily be viewed as an isolated unit of work.

## More Info

https://www.libtorrent.org/udp_tracker_protocol.html
https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
https://www.bittorrent.org/beps/bep_0005.html

## Dependencies

- Linux OS
- cmocka
- libcurl
- valgrind
- GCC

### Quick usage

* make clean
* make all
* make tests
* make valgrind

to run the binary:

./bin/uvgTorrent --magnet_uri="magnet:?xt=urn:btih:etc.etc.etc" --path="/local/download/folder"

#### Author

Simon Bursten <smnbursten@gmail.com>
