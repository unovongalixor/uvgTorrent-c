/*MIT License
Copyright (c) 2017 Dennis Addo
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

/*!
 * @file libipify.c
 *
 *@brief A client library for ipify: A Simple IP Address API in C.
 *
 * @author Dennis Addo ic16b026
 * @see URL: https://github.com/seekaddo/libipify-c$
 *
 * @date 12/1/17.
 *
 * @version 1.0
 *
 * @todo All implentations must be contained in one method structure unless these functions
 *      are required by other programs/methods
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include "ipify.h"

/*! request_templt   The GET request to the ipify host in json*/
static const char *const request_templt = "GET /?format=json HTTP/1.1\r\nHost: api.ipify.org\r\n"
                                          "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_6) AppleWebKit/537.36 (KHTML, "
                                          "like Gecko) Chrome/62.0.3202.94 Safari/537.36\r\n"
                                          "Accept: application/json charset=utf-8\r\n"
                                          "Content-Type: application/json\r\n"
                                          "\r\n\r\n";

/*! ipify_host   the ipify api host to connect with*/
static const char *const ipify_host = "api.ipify.org";


/*<---- user defined variables   --->*/

static int sfd = -1;
static char msbuffer[4069]; /*! maximum bytes for the GET response */

/*! getIPaddr   retreive the ip address from the json*/
static char *getIPaddr(char *str);


/**\brief get the ipify host address and make
 *        full-duplex connection to the ipify server.
 *
 *@param void nothing
 *
 * @return nothing
 * */
void ipify_connect(void) {


    struct addrinfo *client_addr, addrspec;

    memset(&addrspec, 0, sizeof(addrspec));

    addrspec.ai_family = AF_UNSPEC;
    addrspec.ai_socktype = SOCK_STREAM;


    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "%s[socket]: %s", "ipify", strerror(errno));
        exit(EXIT_FAILURE);
    }


    if (getaddrinfo(ipify_host, "http", &addrspec, &client_addr) != 0) {
        fprintf(stderr, "%s[getaddrinfo]: %s", "ipify", strerror(errno));
        freeaddrinfo(client_addr);
        close(sfd);
        exit(EXIT_FAILURE);
    }

    if ((connect(sfd, client_addr->ai_addr, client_addr->ai_addrlen)) == -1) {
        fprintf(stderr, "%s[getaddrinfo]: %s", "ipify", strerror(errno));
        freeaddrinfo(client_addr);
        close(sfd);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(client_addr);

}

/**\brief close the full-duplex connection
 *        to the ipify server.
 *
 *@param void nothing
 *
 * @return nothing
 * */
void ipify_disconnect(void) {

    if (sfd != -1) {
        if (shutdown(sfd, SHUT_RDWR) == -1) {
            fprintf(stderr, "%s[shutdown]: %s", "ipify", strerror(errno));
            close(sfd);
            exit(EXIT_FAILURE);
        }

        close(sfd);
    }

}

/**\brief send Http GET json request to the
 *        ipify host. encoding type is json
 *
 *@param buffer a null-terminated string to store the ip address
 *
 * @return a static global null-terminated string ip-address on success and NULL on failure
 * */

char *ipify_getIP(void) {

    char *ipbuffer = NULL;

    if (send(sfd, request_templt, strlen(request_templt), MSG_OOB) == -1) {
        fprintf(stderr, "%s[send]: %s", "ipify", strerror(errno));
        close(sfd);
        exit(EXIT_FAILURE);
    }

    if (recv(sfd, msbuffer, sizeof(msbuffer), 0) == -1) {
        fprintf(stderr, "%s[send]: %s", "ipify", strerror(errno));
        close(sfd);
        exit(EXIT_FAILURE);
    }
#if 0
    fprintf(stdout,"returned IP: %s\n",msbuffer);
    fprintf(stdout,"mesgbut IP: %s\n",strstr(msbuffer,"\r\n\r\n"));
#endif

    ipbuffer = strstr(msbuffer, "\r\n\r\n");

    if (ipbuffer == NULL) {
        return NULL;
    }

    ipbuffer += strlen("\r\n\r\n");

    return getIPaddr(ipbuffer);

}


/**\brief extract the ip address from the
 *        json response msg
 *
 *@param str string containing a json ip address
 *
 * @return a null-terminated string ip-address on success and NULL on failure
 * */
static char *getIPaddr(char *str) {


    char *ipbuf = NULL;
    char *p = str;
    char *rptr = strstr(p, "ip");

    if (rptr != NULL && strlen(rptr) != strlen(p)) {
        rptr += strcspn(strstr(p, "ip"), ":");
        ipbuf = rptr + 2;
        ipbuf[strlen(ipbuf) - 2] = '\0';
    }

    return ipbuf;
}

