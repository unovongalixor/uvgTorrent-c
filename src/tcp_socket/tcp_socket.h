/**
 * @file tcp_socket/tcp_socket.h
 * @author Simon Bursten <smnbursten@gmail.com>
 *
 * @brief tcp socket provides an interface for working with a non blocking tcp socket
 *        you can write multiple messages into it's write buffer, and when you call send it will attempt to send
 *        it's entire write buffer if it can. upon blocking it will update it's state information about what data
 *        it has already sent so that next time send is called it will continue in the correct location.
 *
 *        write buffer is a linked list, allowing for an arbitrary length write buffer
 *
 * @note it's important to call can_send and can_read before calling send or read
 */
#ifndef UVGTORRENT_C_TCP_SOCKET_H
#define UVGTORRENT_C_TCP_SOCKET_H

struct TcpSocketWriteBuffer {
    void * data;
    size_t data_sent;
    size_t data_size;
    struct TcpSocketWriteBuffer * next;
};

struct TcpSocket {
    int socket;
    size_t write_buffer_length;
    int write_buffer_count;
    struct TcpSocketWriteBuffer * write_buffer_head; // for sending in fifo order
    struct TcpSocketWriteBuffer * write_buffer_tail; // for appending in fifo order
    struct sockaddr * addr;
};

extern struct TcpSocket * tcp_socket_new(struct sockaddr * addr);

extern int tcp_socket_connect(struct TcpSocket * tcp);

extern int tcp_socket_can_send(struct TcpSocket * tcp);

extern int tcp_socket_can_read(struct TcpSocket * tcp);

extern size_t tcp_socket_write(struct TcpSocket * tcp, void * data, size_t data_length);

extern size_t tcp_socket_send(struct TcpSocket * tcp);

extern size_t tcp_socket_read(struct TcpSocket * tcp, void * data, size_t data_length);

extern void tcp_socket_close(struct TcpSocket * tcp);

extern struct TcpSocket * tcp_socket_free(struct TcpSocket * tcp);

#endif //UVGTORRENT_C_TCP_SOCKET_H
