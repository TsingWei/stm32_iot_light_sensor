/**
 * @file      esp8266client.c
 * @author    Atakan S.
 * @date      01/01/2019
 * @version   1.0
 * @brief     ESP8266 Client Mode Driver.
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

#ifndef _ESP8266CLIENT_H_
#define _ESP8266CLIENT_H_

// Includes.
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

// Type definitions.
typedef int32_t ESP82_Result_t;
#define ESP82_ERROR      (-1)
#define ESP82_INPROGRESS (0)
#define ESP82_SUCCESS    (1)
#define ESP82_RECEIVE_NOTHING    (-2)

// Prototypes.
void ESP82_Init(const uint32_t baud, const uint8_t parity, uint32_t (* const getTime_ms_functionHandler)(void));
ESP82_Result_t ESP82_CheckPresence(void);
ESP82_Result_t ESP82_ConnectWifi(const bool resetToDefault, const char * ssid, const char * pass);
ESP82_Result_t ESP82_IsConnectedWifi(void);
ESP82_Result_t ESP82_StartTCP(const char * host, const uint16_t port, const uint16_t keepalive, const bool ssl);
ESP82_Result_t ESP82_CloseTCP(void);
ESP82_Result_t ESP82_Send(const char * const data, const uint8_t dataLength);
ESP82_Result_t ESP82_Receive(char * const data, const uint8_t dataLengthMax);
ESP82_Result_t ESP82_Delay(const uint16_t delay_ms);

#endif
