#include <stdint.h>

#ifndef UVGTORRENT_C_TRACKER_H
#define UVGTORRENT_C_TRACKER_H

struct Tracker {
  char * url;
  int connected;
  uint64_t connection_id;
  uint32_t interval;
  uint32_t seeders;
  uint32_t leechers;
};

extern struct Tracker * tracker_new(char * url);
extern struct Tracker * tracker_free(struct Tracker *);

#endif //UVGTORRENT_C_TORRENT_H
