#include <stdlib.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <inttypes.h>
#include <sys/socket.h>
#include "buffered_socket.h"
#include "../macros.h"
#include "../deadline/deadline.h"

#define BYTES_PER_SECOND_TO_KB_PER_SECOND 0.000976563

struct BufferedSocket * buffered_socket_new(struct sockaddr * addr) {
    struct BufferedSocket * buffered_socket = malloc(sizeof(struct BufferedSocket));
    if(buffered_socket == NULL) {
        throw("tcp socket failed to alloc");
    }

    buffered_socket->socket = -1;
    buffered_socket->opt = 0;
    buffered_socket->write_buffer_head = NULL;
    buffered_socket->write_buffer_tail = NULL;
    buffered_socket->read_buffer = NULL;
    buffered_socket->read_buffer_size = 0;
    buffered_socket->addr = addr;

    buffered_socket->download_rate = 0.00;
    buffered_socket->last_download_rate_update = now();
    buffered_socket->upload_rate = 0.00;
    buffered_socket->last_upload_rate_update = now();

    return buffered_socket;

    error:
    return buffered_socket_free(buffered_socket);
}

int buffered_socket_set_socket_fd(struct BufferedSocket * buffered_socket, int socket_fd) {
    buffered_socket->socket = socket_fd;

    if ((buffered_socket->opt = fcntl(buffered_socket->socket, F_GETFL, NULL)) < 0) {
        return EXIT_FAILURE;
    }

    // set socket non-blocking
    if (fcntl(buffered_socket->socket, F_SETFL, buffered_socket->opt | O_NONBLOCK) < 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int buffered_socket_connect(struct BufferedSocket * buffered_socket) {
    if ((buffered_socket->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return EXIT_FAILURE;
    }

    // get socket flags
    if ((buffered_socket->opt = fcntl(buffered_socket->socket, F_GETFL, NULL)) < 0) {
        return EXIT_FAILURE;
    }

    // set socket non-blocking
    if (fcntl(buffered_socket->socket, F_SETFL, buffered_socket->opt | O_NONBLOCK) < 0) {
        return EXIT_FAILURE;
    }

    if(connect(buffered_socket->socket, buffered_socket->addr, sizeof(struct sockaddr)) < 0) {
        if (errno != EINPROGRESS) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;

}

int buffered_socket_can_write(struct BufferedSocket * buffered_socket) {
    if(buffered_socket != NULL) {
        if (buffered_socket->socket != -1) {
            struct pollfd poll_set[1];
            memset(poll_set, 0x00, sizeof(poll_set));
            poll_set[0].fd = buffered_socket->socket;
            poll_set[0].events = POLLOUT;

            poll(poll_set, 1, 0);

            if (poll_set[0].revents & POLLOUT) {
                return 1;
            }
        }
    }

    return 0;
}

int buffered_socket_can_network_write(struct BufferedSocket * buffered_socket) {
    if(buffered_socket != NULL) {
        if (buffered_socket->socket != -1) {
            return (buffered_socket->write_buffer_head != NULL);
        }
    }
    return 0;
}

int buffered_socket_can_network_read(struct BufferedSocket * buffered_socket) {
    if(buffered_socket != NULL) {
        if (buffered_socket->socket != -1) {
            struct pollfd poll_set[1];
            memset(poll_set, 0x00, sizeof(poll_set));
            poll_set[0].fd = buffered_socket->socket;
            poll_set[0].events = POLLIN;

            poll(poll_set, 1, 0);
            if(poll_set[0].revents & POLLIN) {
                return 1;
            } else {
                return 0;
            }
        }
    }
    return 0;
}


int buffered_socket_can_read(struct BufferedSocket * buffered_socket) {
    if(buffered_socket != NULL) {
        if (buffered_socket->socket != -1) {
            return buffered_socket->read_buffer_size != 0;
        }
    }

    return 0;
}

int buffered_socket_has_hungup(struct BufferedSocket * buffered_socket) {
    if(buffered_socket != NULL) {
        if (buffered_socket->socket != -1) {
            struct pollfd poll_set[1];
            memset(poll_set, 0x00, sizeof(poll_set));
            poll_set[0].fd = buffered_socket->socket;
            poll_set[0].events = POLLIN;
            poll(poll_set, 1, 0);

            if (poll_set[0].revents & POLLHUP) {
                return 1;
            }
        }
    }

    return 0;
}

size_t buffered_socket_write(struct BufferedSocket * buffered_socket, void * data, size_t data_size) {
    struct BufferedSocketWriteBuffer * write_buffer = malloc(sizeof(struct BufferedSocketWriteBuffer));
    if (write_buffer == NULL) {
        throw("failed to write to socket write buffer");
    }
    write_buffer->data = malloc(data_size);
    memset(write_buffer->data, 0x00, data_size);
    memcpy(write_buffer->data, data, data_size);
    write_buffer->data_size = data_size;
    write_buffer->data_sent = 0;
    write_buffer->next = NULL;

    if(buffered_socket->write_buffer_head == NULL & buffered_socket->write_buffer_tail == NULL) {
        buffered_socket->write_buffer_head = write_buffer;
        buffered_socket->write_buffer_tail = write_buffer;
    } else {
        buffered_socket->write_buffer_tail->next = write_buffer;
        buffered_socket->write_buffer_tail = write_buffer;
    }

    return data_size;

    error:
    return -1;
}

size_t buffered_socket_network_write(struct BufferedSocket * buffered_socket) {
    if(buffered_socket->write_buffer_head == NULL) {
        return -1;
    }

    uint64_t last_update = buffered_socket->last_upload_rate_update;
    size_t total_bytes_sent = 0;

    // write anything we need to write
    while(buffered_socket->write_buffer_head != NULL) {
        size_t bytes_to_send = buffered_socket->write_buffer_head->data_size - buffered_socket->write_buffer_head->data_sent;

        int result = write(buffered_socket->socket, buffered_socket->write_buffer_head->data + buffered_socket->write_buffer_head->data_sent, bytes_to_send);
        if(result == -1) {
            return -1;
        } else if(result == 0) {
            return 0;
        } else if(result < bytes_to_send) {
            buffered_socket->write_buffer_head->data_sent += result;
            return -1;
        }

        total_bytes_sent += result;

        struct BufferedSocketWriteBuffer * next = buffered_socket->write_buffer_head->next;
        free(buffered_socket->write_buffer_head->data);
        free(buffered_socket->write_buffer_head);
        buffered_socket->write_buffer_head = NULL;
        buffered_socket->write_buffer_head = next;
    }

    buffered_socket->write_buffer_tail = NULL;

    buffered_socket->last_upload_rate_update = now();
    uint64_t milliseconds_elapsed = ((buffered_socket->last_upload_rate_update - last_update));
    if(milliseconds_elapsed > 0) {
        buffered_socket->upload_rate = (float) total_bytes_sent / ((float) milliseconds_elapsed / 1000);
    } else {
        buffered_socket->upload_rate = 0.00;
    }

    return 1;
}

size_t buffered_socket_network_read(struct BufferedSocket * buffered_socket) {
    /* read whatever we can */
    uint8_t buffer[65535]; // absolute tcp limit
    memset(&buffer, 0x00, sizeof(buffer));

    uint64_t last_update = buffered_socket->last_download_rate_update;

    int read_size = read(buffered_socket->socket, &buffer, sizeof(buffer));
    if(read_size == -1) {
        buffered_socket->download_rate = 0.00;
        return -1;
    } else if (read_size == 0) {
        buffered_socket->download_rate = 0.00;
        return 0;
    }

    buffered_socket->last_download_rate_update = now();
    uint64_t milliseconds_elapsed = (buffered_socket->last_download_rate_update - last_update);
    if(milliseconds_elapsed > 0) {
        buffered_socket->download_rate = (float) read_size / ((float) milliseconds_elapsed / 1000);
    } else {
        buffered_socket->download_rate = 0.00;
    }

    if(buffered_socket->read_buffer_size == 0) {
        buffered_socket->read_buffer = malloc(read_size);
        memcpy(buffered_socket->read_buffer, &buffer, read_size);
        buffered_socket->read_buffer_size = read_size;
    } else {
        size_t new_buffer_size = buffered_socket->read_buffer_size + read_size;
        buffered_socket->read_buffer = realloc(buffered_socket->read_buffer, new_buffer_size);
        memcpy(buffered_socket->read_buffer + buffered_socket->read_buffer_size, &buffer, read_size);
        buffered_socket->read_buffer_size = new_buffer_size;
    }

    return 1;
}

size_t buffered_socket_read(struct BufferedSocket * buffered_socket, void * data, size_t data_length) {
    if(buffered_socket == NULL) {
        goto error;
    } else if(buffered_socket->socket == -1) {
        goto error;
    }
    if(data_length > buffered_socket->read_buffer_size) {
        return 0; // we dont have enough data in memory, treat like timeout
    }
    memcpy(data, buffered_socket->read_buffer, data_length);

    size_t new_buffer_size = buffered_socket->read_buffer_size - data_length;
    if(new_buffer_size == 0) {
        free(buffered_socket->read_buffer);
        buffered_socket->read_buffer = NULL;
        buffered_socket->read_buffer_size = 0;
    } else {
        void * new_read_buffer = malloc(new_buffer_size);
        if(new_read_buffer == NULL) {
            return -1;
        }
        memcpy(new_read_buffer, buffered_socket->read_buffer + data_length, new_buffer_size);
        free(buffered_socket->read_buffer);
        buffered_socket->read_buffer = NULL;

        buffered_socket->read_buffer = new_read_buffer;
        buffered_socket->read_buffer_size = new_buffer_size;
    }

    return data_length;

    error:
    return -1;
}

void buffered_socket_close(struct BufferedSocket * buffered_socket) {
    if (buffered_socket->socket > 0) {
        close(buffered_socket->socket);
        buffered_socket->socket = -1;
    }
}

struct BufferedSocket * buffered_socket_free(struct BufferedSocket * buffered_socket) {
    if(buffered_socket != NULL) {
        buffered_socket_close(buffered_socket);
        if(buffered_socket->read_buffer != NULL) {
            free(buffered_socket->read_buffer);
        }
        if (buffered_socket->write_buffer_head != NULL) {
            struct BufferedSocketWriteBuffer * current = buffered_socket->write_buffer_head;
            while(current != NULL) {
                struct BufferedSocketWriteBuffer * next = current->next;
                free(current->data);
                free(current);
                current = NULL;
                current = next;
            }
        }

        free(buffered_socket);
        buffered_socket = NULL;
    }

    return buffered_socket;
}
