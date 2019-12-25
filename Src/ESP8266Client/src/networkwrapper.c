/**
 * @file      networkwrapper.c
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

// Includes.
#include "networkwrapper.h"
#include "ESP8266Client.h"
#include <string.h>

// Variables.
static char network_host[32] = "10.21.100.103";///< HostName i.e. "test.mosquitto.org"
static unsigned short int network_port = 1883;///< Remote port number.
static unsigned short int network_keepalive = 20;///< Default keepalive time in seconds.
static char network_ssl = false;///< SSL is disabled by default.
static int network_send_state = 0;///< Internal state of send.
static int network_recv_state = 0;///< Internal state of recv.

// Global time provider.
extern long unsigned int network_gettime_ms(void);///< Returns 32bit ms time value.

void network_init(void){ }

void network_close(void){ }

int network_connect(const char * host, const unsigned short int port, const unsigned short int keepalive, const char ssl){
	// Get connection info.
	strcpy(network_host, host);
	network_port = port;
	network_keepalive = keepalive;
	network_ssl = ssl;

	// Reset the internal states.
	network_send_state = 0;
	network_recv_state = 0;

	// Success.
	return 0;
}

int network_send(unsigned char *address, unsigned int bytes){
	// State Machine.
	ESP82_Result_t espResult = ESP82_SUCCESS;
	switch(network_send_state) {
	case 0:
		// Init ESP8266 driver.
		ESP82_Init(115200, false, network_gettime_ms);

		// To the next state.
		network_send_state++;

		break;
	case 1:
		// Connect to wifi (restore to default first).
		#include "wifi_credentials.h"// Has the below 2 definitions only.
		espResult = ESP82_ConnectWifi(true, WIFI_AP_SSID, WIFI_AP_PASS);
		if(espResult == ESP82_SUCCESS){
			// To the next state.
			network_send_state++;
		}
		break;
	case 2:
		// Wait 1sec.
		espResult = ESP82_Delay(1000);
		if(espResult == ESP82_SUCCESS){
			// To the next state.
			network_send_state++;
		}
		break;
	case 3:
		// Check the wifi connection status.
		espResult = ESP82_IsConnectedWifi();
		if(espResult == ESP82_SUCCESS){
			// To the next state.
			network_send_state++;
		}
		break;
	case 4:
		// Start TCP connection.
		espResult = ESP82_StartTCP(network_host, network_port, network_keepalive, network_ssl);
		if(espResult == ESP82_SUCCESS){
			// To the next state.
			network_send_state++;
		}
		break;
	case 5:
		// Send the data.
		espResult = ESP82_Send(address, bytes);
		if(espResult == ESP82_SUCCESS){
			// Return the actual number of bytes. Stay in this state unless error occurs.
			return bytes;
		}
		break;
	default:
		// Reset the state machine.
		network_send_state = 0;
	}

	// Fall-back on error.
	if(espResult == ESP82_ERROR){
		if(network_send_state < 4){
			// If error occured before wifi connection, start over.
			network_send_state = 0;
		}else{
			// Check wifi connection and try to send again.
			network_send_state = 3;
		}

		// Error.
		return -1;
	}

	// In progress.
	return 0;
}

int network_recv(unsigned char *address, unsigned int maxbytes){
	static char receiveBuffer[128];
	static int receiveBufferBack = 0;
	static int receiveBufferFront = 0;
	int actualLength;

	// State Machine.
	ESP82_Result_t espResult;
	switch(network_recv_state) {
	case 0:
		espResult = ESP82_Receive(receiveBuffer, 128);
		if(espResult > 0){
			// Set the buffer pointers.
			receiveBufferBack = espResult;
			receiveBufferFront = 0;

			// To the next state.
			network_recv_state++;
		}
		break;
	case 1:
		// Extract to the out buffer.
		if(receiveBufferFront < receiveBufferBack) {
			// Get actual length.
			actualLength = (receiveBufferBack - receiveBufferFront);
			if(actualLength > maxbytes){
				actualLength = maxbytes;
			}

			// Extract the actual bytes.
			memcpy(address, &receiveBuffer[receiveBufferFront], actualLength);
			receiveBufferFront += actualLength;

			// Buffer is empty.
			if(receiveBufferBack == receiveBufferFront) {
				network_recv_state = 0;
			}

			// Return the count.
			return actualLength;
		}
		break;
	default:
		// Reset the state machine.
		network_recv_state = 0;
	}

	// Fall-back on error.
	if(espResult == ESP82_ERROR){
		// Reset the state machine.
		network_recv_state = 0;
		
		// Error.
		return -1;
	}
	// Recv nothing.
	if(espResult == ESP82_RECEIVE_NOTHING){
		// Reset the state machine.
		network_recv_state = 0;

		return -2;
	}


	// In progress.
	return 0;
}
