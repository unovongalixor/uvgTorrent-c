#include <stdlib.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <inttypes.h>
#include <sys/socket.h>
#include "tcp_socket.h"
#include "../macros.h"

struct TcpSocket * tcp_socket_new(struct sockaddr * addr) {
    struct TcpSocket * tcp = malloc(sizeof(struct TcpSocket));
    if(tcp == NULL) {
        throw("tcp socket failed to alloc");
    }

    tcp->socket = -1;
    tcp->opt = 0;
    tcp->write_buffer_head = NULL;
    tcp->write_buffer_tail = NULL;
    tcp->read_buffer = NULL;
    tcp->read_buffer_size = 0;
    tcp->addr = addr;

    return tcp;

    error:
    return tcp_socket_free(tcp);
}

int tcp_socket_connect(struct TcpSocket * tcp) {
    if ((tcp->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return EXIT_FAILURE;
    }

    // get socket flags
    if ((tcp->opt = fcntl(tcp->socket, F_GETFL, NULL)) < 0) {
        return EXIT_FAILURE;
    }

    // set socket non-blocking
    if (fcntl(tcp->socket, F_SETFL, tcp->opt | O_NONBLOCK) < 0) {
        return EXIT_FAILURE;
    }

    if(connect(tcp->socket, tcp->addr, sizeof(struct sockaddr)) < 0) {
        if (errno != EINPROGRESS) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;

}

int tcp_socket_can_write(struct TcpSocket * tcp) {
    if(tcp != NULL) {
        if (tcp->socket != -1) {
            struct pollfd poll_set[1];
            memset(poll_set, 0x00, sizeof(poll_set));
            poll_set[0].fd = tcp->socket;
            poll_set[0].events = POLLOUT;

            poll(poll_set, 1, 1);

            if (poll_set[0].revents & POLLOUT) {
                return 1;
            }
        }
    }

    return 0;
}

int tcp_socket_can_network_write(struct TcpSocket * tcp) {
    if(tcp != NULL) {
        if (tcp->socket != -1) {
            return (tcp->write_buffer_head != NULL);
        }
    }
    return 0;
}

int tcp_socket_can_network_read(struct TcpSocket * tcp) {
    if(tcp != NULL) {
        if (tcp->socket != -1) {
            struct pollfd poll_set[1];
            memset(poll_set, 0x00, sizeof(poll_set));
            poll_set[0].fd = tcp->socket;
            poll_set[0].events = POLLIN;

            poll(poll_set, 1, 1);

            return poll_set[0].revents & POLLIN;
        }
    }
    return 0;
}


int tcp_socket_can_read(struct TcpSocket * tcp) {
    if(tcp != NULL) {
        if (tcp->socket != -1) {
            return tcp->read_buffer_size != 0;
        }
    }

    return 0;
}

size_t tcp_socket_write(struct TcpSocket * tcp, void * data, size_t data_size) {
    struct TcpSocketWriteBuffer * write_buffer = malloc(sizeof(struct TcpSocketWriteBuffer));
    if (write_buffer == NULL) {
        throw("failed to write to socket write buffer");
    }
    write_buffer->data = malloc(data_size);
    memset(write_buffer->data, 0x00, data_size);
    memcpy(write_buffer->data, data, data_size);
    write_buffer->data_size = data_size;
    write_buffer->data_sent = 0;
    write_buffer->next = NULL;

    if(tcp->write_buffer_head == NULL & tcp->write_buffer_tail == NULL) {
        tcp->write_buffer_head = write_buffer;
        tcp->write_buffer_tail = write_buffer;
    } else {
        tcp->write_buffer_tail->next = write_buffer;
        tcp->write_buffer_tail = write_buffer;
    }

    return data_size;

    error:
    return -1;
}

size_t tcp_socket_network_write(struct TcpSocket * tcp) {
    if(tcp->write_buffer_head == NULL) {
        return 0;
    }
    int result;

    // write anything we need to write
    while(tcp->write_buffer_head != NULL) {
        size_t bytes_to_send = tcp->write_buffer_head->data_size - tcp->write_buffer_head->data_sent;

        result = write(tcp->socket, tcp->write_buffer_head->data + tcp->write_buffer_head->data_sent, bytes_to_send);
        if(result == -1) {
            if (errno == EAGAIN | errno == EWOULDBLOCK) {
                return 0;
            } else {
                return -1;
            }
        } else if(result < bytes_to_send) {
            tcp->write_buffer_head->data_sent += result;
            return 0;
        }

        struct TcpSocketWriteBuffer * next = tcp->write_buffer_head->next;
        free(tcp->write_buffer_head->data);
        free(tcp->write_buffer_head);
        tcp->write_buffer_head = NULL;
        tcp->write_buffer_head = next;
    }

    tcp->write_buffer_tail = NULL;

    return 1;
}

size_t tcp_socket_network_read(struct TcpSocket * tcp) {
    /* read whatever we can */
    uint8_t buffer[65535]; // absolute tcp limit
    memset(&buffer, 0x00, sizeof(buffer));

    int read_size = read(tcp->socket, &buffer, sizeof(buffer));
    if(read_size == -1) {
        if (errno == EAGAIN | errno == EWOULDBLOCK) {
            return 0;
        } else {
            return -1;
        }
    } else if (read_size == 0) {
        return 0;
    }

    if(tcp->read_buffer_size == 0) {
        tcp->read_buffer = malloc(read_size);
        memcpy(tcp->read_buffer, &buffer, read_size);
        tcp->read_buffer_size = read_size;
    } else {
        size_t new_buffer_size = tcp->read_buffer_size + read_size;
        tcp->read_buffer = realloc(tcp->read_buffer, new_buffer_size);
        memcpy(tcp->read_buffer + tcp->read_buffer_size, &buffer, read_size);
        tcp->read_buffer_size = new_buffer_size;
    }

    return 1;
}

size_t tcp_socket_read(struct TcpSocket * tcp, void * data, size_t data_length) {
    if(data_length > tcp->read_buffer_size) {
        return 0; // we dont have enough data in memory, treat like timeout
    }

    memcpy(data, tcp->read_buffer, data_length);

    size_t new_buffer_size = tcp->read_buffer_size - data_length;
    if(new_buffer_size == 0) {
        free(tcp->read_buffer);
        tcp->read_buffer = NULL;
        tcp->read_buffer_size = 0;
    } else {
        void * new_read_buffer = malloc(new_buffer_size);
        if(new_read_buffer == NULL) {
            return -1;
        }
        memcpy(new_read_buffer, tcp->read_buffer + data_length, new_buffer_size);
        free(tcp->read_buffer);
        tcp->read_buffer = NULL;

        tcp->read_buffer = new_read_buffer;
        tcp->read_buffer_size = new_buffer_size;
    }

    return data_length;
}

void tcp_socket_close(struct TcpSocket * tcp) {
    if (tcp->socket > 0) {
        close(tcp->socket);
        tcp->socket = -1;
    }
}

struct TcpSocket * tcp_socket_free(struct TcpSocket * tcp) {
    if(tcp != NULL) {
        tcp_socket_close(tcp);
        if(tcp->read_buffer != NULL) {
            free(tcp->read_buffer);
        }
        if (tcp->write_buffer_head != NULL) {
            struct TcpSocketWriteBuffer * current = tcp->write_buffer_head;
            while(current != NULL) {
                struct TcpSocketWriteBuffer * next = current->next;
                free(current->data);
                free(current);
                current = NULL;
                current = next;
            }
        }

        free(tcp);
        tcp = NULL;
    }

    return tcp;
}
