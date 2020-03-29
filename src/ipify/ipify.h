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
 * @file libipify.h
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

#ifndef IPIFY_H
#define IPIFY_H


extern void ipify_connect(void);
extern void ipify_disconnect(void);
extern char *ipify_getIP(void);


#endif //LIBIPIFY_C_LIBIPIFY_H
