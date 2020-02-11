/**
 * @file buffered_socket/buffered_socket.h
 * @author Simon Bursten <smnbursten@gmail.com>
 *
 * @brief the buffered_socket provides a buffered interface for non-blocking sockets. in contrast to blocking sockets,
 *        non-blocking sockets require you to handle cases where you only receive a portion of the data you were
 *        attempting to read. this struct aims to simplify those cases.
 *
 *        calling buffered_socket_network_read will attempt to read as much data as possible from the network into the
 *        buffered_sockets read buffer. a subsequent buffered_socket_read call will return -1 in case of an error, 0 in case
 *        there wasn't enough data available and the size of the data read in case of success. it will either return
 *        nothing, or the entire amount of requested data, while never blocking.
 *
 *        calling buffered_socket_write will write data into the buffered_sockets write buffer. a subsequent
 *        buffered_socket_network_write call will return -1 in case of error and the amount of data written in case
 *        of success.
 *
 *  @note in situations where you make multiple reads to handle a single message you will need to deal with cases
 *        where the first read succedes and the second fails. you can see an example of this in peer/peer.h in the function
 *        peer_read_message. one iteration may successfully read msg_length and the next may read msg_id. the function
 *        is constructed to succede over a number of iterations.
 *
 *  @example while(running) {
 *              if(buffered_socket_can_network_read(socket) == 1) {
 *                  // attempt to read from network into buffer
 *                  buffered_socket_network_read(socket);
 *              }
 *              if(buffered_socket_can_network_write(socket) == 1) {
 *                  // attempt to write from buffer to network
 *                  buffered_socket_network_write(socket);
 *              }
 *              if(buffered_socket_can_write(socket)){
 *                  // attempt to write a message to buffer
 *                  uint32_t message = 0xFFFFFFFF;
 *                  int res = buffered_socket_write(socket, &message, sizeof(message));
 *                  if (res == -1) {
 *                      // error
 *                  } else if(res == sizeof(message)) {
 *                      // success
 *                  }
 *              }
 *              if(buffered_socket_can_read(socket)){
 *                  // attempt to read a message from buffer
 *                  uint32_t message;
 *                  int res = buffered_socket_read(socket, &message, sizeof(message));
 *                  if (res == -1) {
 *                      // error
 *                  } else if(res == 0) {
 *                      // not yet enough data available, try again next time
 *                  } else if(res == sizeof(message)) {
 *                      // success
 *                  }
 *              }
 *           }
 *
 */
#ifndef UVGTORRENT_C_BUFFERED_SOCKET_H
#define UVGTORRENT_C_BUFFERED_SOCKET_H

struct BufferedSocketWriteBuffer {
    void * data;
    size_t data_sent;
    size_t data_size;
    struct BufferedSocketWriteBuffer * next;
};

struct BufferedSocket {
    /* socket stuff */
    int opt;
    int socket;
    struct sockaddr * addr;

    /* write buffer */
    struct BufferedSocketWriteBuffer * write_buffer_head; // for sending in fifo order
    struct BufferedSocketWriteBuffer * write_buffer_tail; // for appending in fifo order

    /* read bu*/
    void * read_buffer;
    size_t read_buffer_size;
};

extern struct BufferedSocket * buffered_socket_new(struct sockaddr * addr);

extern int buffered_socket_connect(struct BufferedSocket * buffered_socket);

extern int buffered_socket_can_write(struct BufferedSocket * buffered_socket);

extern int buffered_socket_can_network_write(struct BufferedSocket * buffered_socket);

extern int buffered_socket_can_network_read(struct BufferedSocket * buffered_socket);

extern int buffered_socket_can_read(struct BufferedSocket * buffered_socket);

extern size_t buffered_socket_write(struct BufferedSocket * buffered_socket, void * data, size_t data_length);

extern size_t buffered_socket_network_write(struct BufferedSocket * buffered_socket);

extern size_t buffered_socket_network_read(struct BufferedSocket * buffered_socket);

extern size_t buffered_socket_read(struct BufferedSocket * buffered_socket, void * data, size_t data_length);

extern void buffered_socket_close(struct BufferedSocket * buffered_socket);

extern struct BufferedSocket * buffered_socket_free(struct BufferedSocket * buffered_socket);

#endif //UVGTORRENT_C_BUFFERED_SOCKET_H
