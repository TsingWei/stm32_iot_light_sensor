/**
 * @file      networkwrapper.h
 * @author    Atakan S.
 * @date      01/01/2019
 * @version   1.0
 * @brief     Network wrapper for PAHO MQTT project based on ESP8266.
 *
 * @copyright Copyright (c) 2018 Atakan SARIOGLU ~ www.atakansarioglu.com
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#ifndef _NETWORKWRAPPER_H
#define _NETWORKWRAPPER_H

// Socket data type.
#ifndef network_socket_t
#define network_socket_t void*
#endif

/*
 * @brief Initialize network subsystem.
 */
void network_init(void);

/*
 * @brief Close network connection.
 */
void network_close(void);

/*
 * @brief Starts the connection or saves the parameters and defers the connection to the send/recv methods.
 * @param host Host name.
 * @param port Remote port number.
 * @param keepalive Seconds for keepalive function.
 * @param ssl If true, SSL connection type will be used, otherwise TCP.
 * @return Returns 0 on success and negative on error.
 */
int network_connect(const char * host, const unsigned short int port, const unsigned short int keepalive, const char ssl);

/*
 * @brief NON-BLOCKING Sends data and mimics transparency.
 * @param address Pointer to the data to be sent.
 * @param bytes Number of bytes to be sent.
 * @return Returns the number of actual data bytes sent or negative on error.
 */
int network_send(unsigned char *address, unsigned int bytes);

/*
 * @brief NON-BLOCKING Receives data and mimics transparency.
 * @param address Pointer to the memory into that the received data is stored.
 * @param maxbytes Number of maximum bytes to be received.
 * @return Returns the number of actual data bytes received or negative on error.
 */
int network_recv(unsigned char *address, unsigned int maxbytes);

#endif
