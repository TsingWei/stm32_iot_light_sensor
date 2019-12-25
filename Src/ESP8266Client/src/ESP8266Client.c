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

// Includes.
#include "ESP8266Client.h"
#include <string.h>
#include <assert.h>
#include "usart.h"
#include "fifo.h"

// Timing settings.
#define ESP82_TIMEOUT_MS_CMD           2500UL///< Command sending and processing timeout.
#define ESP82_TIMEOUT_MS_RECEIVE      15000UL///< Data receiving timeout.
#define ESP82_TIMEOUT_MS_DATA_SEND    15000UL///< Data sending timeout.
#define ESP82_TIMEOUT_MS_RESTART       2000UL///< Module restart timeout.
#define ESP82_TIMEOUT_MS_AP_CONNECT   20000UL///< AP connecting timeout.
#define ESP82_TIMEOUT_MS_HOST_CONNECT 10000UL///< Host connecting timeout.

// Buffer settings.
#define ESP82_BUFFERSIZE_UART_2N 11UL///< Buffer size of 2^7=128byte
#define ESP82_BUFFERSIZE_UART (1UL << ESP82_BUFFERSIZE_UART_2N)
#define ESP82_BUFFERSIZE_RESPONSE 1024UL
#define ESP82_BUFFERSIZE_CMD 128UL

// ESP82 Events.
#define ESP82_RES_OK               (1UL<<0)
#define ESP82_RES_ERROR            (1UL<<1)
#define ESP82_RES_FAIL             (1UL<<2)
#define ESP82_RES_BUSY             (1UL<<3)
#define ESP82_RES_WIFI_CONNECTED   (1UL<<4)
#define ESP82_RES_WIFI_GOTIP       (1UL<<5)
#define ESP82_RES_STATUS_GOTIP     (1UL<<6)
#define ESP82_RES_WIFI_DISCONNECT  (1UL<<7)
#define ESP82_RES_SEND_OK          (1UL<<8)
#define ESP82_RES_SEND_BEGIN       (1UL<<9)
#define ESP82_RES_CLOSED           (1UL<<10)
#define ESP82_RES_TIMEOUT          (1UL<<31)///< Indicates a previously occured timeout event.

// ESP82 Event strings.
static const char * ESP82_RES_OK_str = "OK";
static const char * ESP82_RES_ERROR_str = "ERROR";
static const char * ESP82_RES_FAIL_str = "FAIL";
static const char * ESP82_RES_BUSYP_str = "busy p...";
static const char * ESP82_RES_BUSYS_str = "busy s...";
static const char * ESP82_RES_WIFI_GOTIP_str = "WIFI GOT IP";
static const char * ESP82_RES_WIFI_CONNECTED_str = "WIFI CONNECTED";
static const char * ESP82_RES_WIFI_DISCONNECT_str = "WIFI DISCONNECT";
static const char * ESP82_RES_SEND_OK_str = "SEND OK";
static const char * ESP82_RES_STATUS_GOTIP_str = "STATUS:2";
static const char * ESP82_RES_CLOSED_str = "CLOSED";
static const char * ESP82_RES_SEND_BEGIN_str = "\r\n> ";

// Variables.
static unsigned long int (* ESP82_getTime_ms)(void);///< Used to hold handler for time provider.
static char ESP82_uartTxBuf[ESP82_BUFFERSIZE_UART];///< Buffer for circular uart tx.
static char ESP82_uartRxBuf[ESP82_BUFFERSIZE_UART];///< Buffer for circular uart rx.
static char ESP82_resBuffer[ESP82_BUFFERSIZE_RESPONSE]; ///< Buffer to store the response.
static uint8_t ESP82_resBufferFront;///< Buffer front pointer.
static uint8_t ESP82_resBufferBack;///< Buffer back pointer.
static char ESP82_cmdBuffer[ESP82_BUFFERSIZE_CMD];
static uint32_t ESP82_receivedFlags;///< Used for debug purposes.
static bool ESP82_inProgress = false;///< State flag for non-blocking functions.
static void * ESP82_SR_State = NULL;///< State flag for non-blocking functions.
static const char * ESP82_SSLSIZE_str = "AT+CIPSSLSIZE=4096\r\n";///< ESP8266 module memory (2048 to 4096) reserved for SSL.
static unsigned long int ESP82_t0;///< Keeps entry time for timeout detection.

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart1;
extern uint8_t rxBuffer[RX_BUFFER_SIZE];
extern uint8_t debugSentBuffer[RX_BUFFER_SIZE];
extern int recv_end_flag;
extern int rx_len;
struct fifo rxFifo;

// Internal states.
typedef enum {
	ESP82_State0 = 0,
	ESP82_State1,
	ESP82_State2,
	ESP82_State3,
	ESP82_State4,
	ESP82_State5,
	ESP82_State6,
	ESP82_State7,
	ESP82_State8,
	ESP82_State9,
	ESP82_StateMAX,
} tESP82_State;

/*
 * @brief INTERNAL Timeout setup.
 */
static void ESP82_timeoutBegin(void){
	// Get entry time.
	ESP82_t0 = ESP82_getTime_ms();
}

/*
 * @brief INTERNAL Timeout checker.
 * @param interval_ms Interval time in ms.
 * @return True if timeout expired.
 */
static bool ESP82_timeoutIsExpired(const uint16_t interval_ms) {
	// Check if the given interval is in the past.
	return (interval_ms < (ESP82_getTime_ms() - ESP82_t0));
}

/*
 * @brief INTERNAL Sends command to the module.
 * @param command The string command.
 * @param commandLength Length of the command.
 * @param clearBuffers UART and response buffers are cleared if this is true.
 */
static void ESP82_sendCmd(const char * command, const uint8_t commandLength, const bool clearBuffers){
	// Check if restart requested.
	if(clearBuffers){
		// Reset RX+TX buffers and start TX.
		// CircularUART_ClearRx();
		// CircularUART_ClearTx();
		ESP82_resBufferFront = 0;
		ESP82_resBufferBack = 0;
		ESP82_receivedFlags = 0;
	}

	// Write to uart.
	// CircularUART_Send(command, commandLength);
	debugSentBuffer[0] = '\n';
	debugSentBuffer[1] = 'U';
	debugSentBuffer[2] = 'T';
	debugSentBuffer[3] = ':';
	debugSentBuffer[4] = ' ';
	memcpy(debugSentBuffer+5,command,commandLength);
	HAL_UART_Transmit_DMA(&huart2,command,commandLength);
	HAL_UART_Transmit_DMA(&huart1, debugSentBuffer, commandLength+5);
}

/*
 * @brief INTERNAL Gets a CR-LF terminated line from the module response.
 * @param data Module response.
 * @param searchPosition The position to start search. Modified after call.
 * @return Pointer to the line string (terminated).
 */
static char * ESP82_readLine(char * const data, uint8_t * const searchPosition){
	char * posFound;
	uint8_t iterator;

	// Get search starting position.
	iterator = (searchPosition == NULL) ? 0 : *searchPosition;

	// Get "\r\n" position if exists.
	if (NULL != (posFound = strstr(&data[iterator], "\r\n"))) {
		// Terminate the string.
		posFound[0] = 0;

		// Export the new search starting position.
		if (searchPosition) {
			*searchPosition = (posFound - data) + 2;
		}

		// Found.
		return &data[iterator];
	}

	// Not found.
	return NULL;
}



/*
 * @brief INTERNAL Reads command response from the module and checks for the expected events.
 * @param expectedFlags The flag(s) to check.
 * @param timeout_ms Maximum execution time in ms.
 * @param responseOut The response is copied to this pointer when it is not NULL.
 * @param responseLengthMax Maximum response length to copy.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
static ESP82_Result_t ESP82_checkResponse(const uint32_t expectedFlags, const uint16_t timeout_ms, char * const responseOut, const uint8_t responseLengthMax){
	// Switch waiting state.
	if(!ESP82_inProgress) {
		// Start timeout.
		ESP82_timeoutBegin();
	}

	// Get response data and terminate as a string.
	ESP82_resBufferBack += fifo_out(&rxFifo, &ESP82_resBuffer[ESP82_resBufferBack], ESP82_BUFFERSIZE_RESPONSE - 1 - ESP82_resBufferBack);
	ESP82_resBuffer[ESP82_resBufferBack] = 0;
	recv_end_flag == 0;
	// Search for Begin Cursor '>'.
	if((expectedFlags & ESP82_RES_SEND_BEGIN) && (ESP82_resBufferBack >= 4)){
		// Check for the cursor string.
		if(!memcmp(&ESP82_resBuffer[ESP82_resBufferBack - 4], ESP82_RES_SEND_BEGIN_str, 4)){
			ESP82_receivedFlags |= ESP82_RES_SEND_BEGIN;
		}
	}

	// Parse line-by-line and search for known state messages.
	else{
		static char * lineString;
		while((lineString = ESP82_readLine(ESP82_resBuffer, &ESP82_resBufferFront))){
			// Check for error.
			if(!strcmp(lineString, ESP82_RES_OK_str)){
				ESP82_receivedFlags |= ESP82_RES_OK;
			}else

			// Check for error.
			if(!strcmp(lineString, ESP82_RES_ERROR_str)){
				ESP82_receivedFlags |= ESP82_RES_ERROR;
			}else

			// Check for fail.
			if(!strcmp(lineString, ESP82_RES_FAIL_str)){
				ESP82_receivedFlags |= ESP82_RES_FAIL;
			}else

			// Check for busy.
			if(!strcmp(lineString, ESP82_RES_BUSYP_str) || !strcmp(lineString, ESP82_RES_BUSYS_str)){
				ESP82_receivedFlags |= ESP82_RES_BUSY;
			}else

			// Check wifi connected.
			if((expectedFlags & ESP82_RES_WIFI_CONNECTED) && !strcmp(lineString, ESP82_RES_WIFI_CONNECTED_str)){
				ESP82_receivedFlags |= ESP82_RES_WIFI_CONNECTED;
			}else

			// Check wifi gotip.
			if((expectedFlags & ESP82_RES_WIFI_GOTIP) && !strcmp(lineString, ESP82_RES_WIFI_GOTIP_str)){
				ESP82_receivedFlags |= ESP82_RES_WIFI_GOTIP;
			}else

			// Check status gotip.
			if((expectedFlags & ESP82_RES_STATUS_GOTIP) && !strcmp(lineString, ESP82_RES_STATUS_GOTIP_str)){
				ESP82_receivedFlags |= ESP82_RES_STATUS_GOTIP;
			}else

			// Check send ok.
			if((expectedFlags & ESP82_RES_SEND_OK) && !strcmp(lineString, ESP82_RES_SEND_OK_str)){
				ESP82_receivedFlags |= ESP82_RES_SEND_OK;

				// Break here to keep the received data if any.
				break;
			}else
	
			// Check connection closed.
			if((expectedFlags & ESP82_RES_CLOSED) && !strcmp(lineString, ESP82_RES_CLOSED_str)){
				ESP82_receivedFlags |= ESP82_RES_CLOSED;
			}
		}
	}

	// Error, fail or busy.
	if(ESP82_receivedFlags & (ESP82_RES_ERROR | ESP82_RES_FAIL | ESP82_RES_BUSY)){
		// Error.
		ESP82_inProgress = false;
		return ESP82_ERROR;
	}else

	// Check for OK response message frame.
	if(((expectedFlags & ESP82_receivedFlags) == expectedFlags)){
		// Provide the response if requested.
		if(responseOut != NULL){
			// Set the length to copy to the output.
			uint8_t copyLength = ESP82_resBufferFront;

			// Limit length of output.
			if(copyLength > responseLengthMax){
				copyLength = responseLengthMax;
			}

			// Export the response: restore CRs back.
			for(uint8_t i = 0; i < copyLength; i++){
				char c = ESP82_resBuffer[i];
				responseOut[i] = c ? c : '\r';
			}

			// Place string termination.
			if(copyLength < responseLengthMax){
				responseOut[copyLength] = 0;
			}
		}

		// Success.
		ESP82_inProgress = false;
		return ESP82_SUCCESS;
	}else

	// Check for timeout.
	if(ESP82_timeoutIsExpired(timeout_ms)){
		// Fail.
		ESP82_receivedFlags = ESP82_RES_TIMEOUT | expectedFlags;
		ESP82_inProgress = false;
		return ESP82_ERROR;
	}

	// Waiting.
	ESP82_inProgress = true;
	return ESP82_INPROGRESS;
}

/*
 * @brief INTERNAL Sends data after the cursor '>' detection and gets module confirmation message.
 * @param data The data to send.
 * @param dataLength Length of the data.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
static ESP82_Result_t ESP82_sendData(const char * data, const uint8_t dataLength){
	static uint8_t internalState;
	ESP82_Result_t result;

	// State machine.
	switch (internalState = (ESP82_inProgress ? internalState : ESP82_State0)) {
	case ESP82_State0:
		// Check for send-begin cursor '>'.
		if (ESP82_SUCCESS == (result = ESP82_checkResponse(ESP82_RES_SEND_BEGIN, ESP82_TIMEOUT_MS_CMD, NULL, 0))) {
			// Send the data.
			ESP82_sendCmd(data, dataLength, true);

			// Switch to waiting for SEND OK.
			internalState = ESP82_State1;
		} else {
			// In progress or failure.
			return result;
		}

		//nobreak;
	case ESP82_State1:
		// Wait for SEND OK.
		return ESP82_checkResponse(ESP82_RES_SEND_OK, ESP82_TIMEOUT_MS_DATA_SEND, NULL, 0);
	}
}

/*
 * @brief INTERNAL Executes a command and checks the result via the flags.
 * @param command The null-terminated string command.
 * @param expectedFlags The flag(s) to check.
 * @param timeout_ms Maximum execution time in ms.
 * @param responseOut The response is copied to this pointer when it is not NULL.
 * @param responseLengthMax Maximum response length to copy.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
static ESP82_Result_t ESP82_execute(const char * command, const uint32_t expectedFlags, const uint16_t timeout_ms, char * const response, const uint8_t responseLengthMax){
	static uint8_t internalState;

	// State machine.
	switch (internalState = (ESP82_inProgress ? internalState : ESP82_State0)) {
	case ESP82_State0:
		// Send.
		ESP82_sendCmd(command, strlen(command), true);

		// To the next state.
		internalState = ESP82_State1;

		//nobreak;
	case ESP82_State1:
		// Wait for response.
		return ESP82_checkResponse(expectedFlags, timeout_ms, response, responseLengthMax);
	}
}

/*
 * @brief Creates non-blocking delay.
 * @param delay_ms Delay time in ms.
 * @return SUCCESS, INPROGRESS.
 */
ESP82_Result_t ESP82_Delay(const uint16_t delay_ms){
	// Function entry.
	if(!ESP82_inProgress){
		// Start timeout.
		ESP82_timeoutBegin();
	}

	// Check delay time expiry.
	return (ESP82_Result_t)!(ESP82_inProgress = !ESP82_timeoutIsExpired(delay_ms));
}

/*
 * @brief Initialize the UART and the module.
 * @param baud UART baud-rate.
 * @param parity UART parity setting (0:no-parity, 1:odd, 2:even).
 * @param getTime_ms_functionHandler Function handler for getting time in ms.
 */
void ESP82_Init(const uint32_t baud, const uint8_t parity, uint32_t (* const getTime_ms_functionHandler)(void)) {
	// Get the time provider.
	ESP82_getTime_ms = getTime_ms_functionHandler;

	// Reset internal state machines.
	ESP82_inProgress = false;
}

/*
 * @brief Check if the module is responsive.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
ESP82_Result_t ESP82_CheckPresence(void) {
	return ESP82_ConnectWifi(false, NULL, NULL);
}

/*
 * @brief Connect to AP.
 * @param resetToDefault If true, reset the module to default settings before connecting.
 * @param ssid AP name.
 * @param pass AP password.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
ESP82_Result_t ESP82_ConnectWifi(const bool resetToDefault, const char * ssid, const char * pass) {
	static uint8_t internalState;
	ESP82_Result_t result;

	// State machine.
	switch (internalState = (ESP82_inProgress ? internalState : ESP82_State0)) {
	case ESP82_State0:
		// Wait for startup phase to finish.
		if(ESP82_SUCCESS == (result = ESP82_Delay(ESP82_TIMEOUT_MS_RESTART))) {
			// To the next state.
			internalState = ESP82_State1;
		} else {
			// INPROGRESS or SUCCESS if no reset is requested.
			return result;
		}

		//nobreak;
	case ESP82_State1:
		// AT+RESTORE (if requested).
		if(!resetToDefault || (ESP82_SUCCESS == (result = ESP82_execute("AT+RESTORE\r\n", ESP82_RES_OK, ESP82_TIMEOUT_MS_CMD, NULL, 0)))) {
			// To the next state.
			internalState = ESP82_State2;
		} else {
			// Exit on ERROR or INPROGRESS.
			return result;
		}

		//nobreak;
	case ESP82_State2:
		// If resetted, wait for restart to finish.
		if(!resetToDefault || (ESP82_SUCCESS == (result = ESP82_Delay(ESP82_TIMEOUT_MS_RESTART)))){
				// To the next state.
				internalState = ESP82_State3;
		}else{
			// INPROGRESS or SUCCESS if no reset is requested.
			return result;
		}

		//nobreak;
	case ESP82_State3:
		// AT+CWMODE (client mode)
		if((ESP82_SUCCESS == (result = ESP82_execute("AT+CWMODE=1\r\n", ESP82_RES_OK, ESP82_TIMEOUT_MS_CMD, NULL, 0))) && (ssid != NULL)){
			// To the next state.
			internalState = ESP82_State4;
		} else{
			// Exit on ERROR, INPROGRESS or SUCCESS (if no SSID is provided).
			return result;
		}

		// nobreak;
	case ESP82_State4:
		// Size check.
		if ((strlen(ssid) + strlen(pass)) > (ESP82_BUFFERSIZE_CMD - 17)) {
			return false;
		}

		// AT+CWJAP prepare.
  		sprintf(ESP82_cmdBuffer, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pass);
   
		// To the next state.
		internalState = ESP82_State5;

		//nobreak;
	case ESP82_State5:
		// AT+CWJAP
		return ESP82_execute(ESP82_cmdBuffer, (ESP82_RES_OK | ESP82_RES_WIFI_CONNECTED | ESP82_RES_WIFI_GOTIP), ESP82_TIMEOUT_MS_AP_CONNECT, NULL, 0);

		//nobreak;
	default:
		// To the first state.
		internalState = ESP82_State0;
	}
}

/*
 * @brief Connection test.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
ESP82_Result_t ESP82_IsConnectedWifi(void) {
	return ESP82_execute("AT+CIPSTATUS\r\n", ESP82_RES_STATUS_GOTIP, ESP82_TIMEOUT_MS_CMD, NULL, 0);
}

/*
 * @brief Connect to server via TCP.
 * @param host Hostname or IP address.
 * @param port Remote port.
 * @param keepalive Keep-alive time between 0 to 7200 seconds.
 * @param ssl Starts SSL connection.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
ESP82_Result_t ESP82_StartTCP(const char * host, const uint16_t port, const uint16_t keepalive, const bool ssl) {
	static uint8_t internalState;
	ESP82_Result_t result;

	// State machine.
	switch (internalState = (ESP82_inProgress ? internalState : ESP82_State0)) {
	case ESP82_State0:
		// Size check.
		if(strlen(host) > (ESP82_BUFFERSIZE_CMD - 34)){
			return false;
		}

		// Keepalive check.
		if(keepalive > 7200){
			return false;
		}

		// prepare AT+CIPSTART
		sprintf(ESP82_cmdBuffer, "AT+CIPSTART=\"%s\",\"%s\",%i,%i\r\n", (ssl ? "SSL" : "TCP"), host, port, keepalive);

		// To the next state.
		internalState = ESP82_State1;

		//nobreak;
	case ESP82_State1:
		// AT+CIPSSLSIZE (or skip)
		if(!ssl || (ESP82_SUCCESS == (result = ESP82_execute(ESP82_SSLSIZE_str, ESP82_RES_OK, ESP82_TIMEOUT_MS_CMD, NULL, 0)))){
			// To the next state.
			internalState = ESP82_State2;
		}else{
			// Exit on ERROR or INPROGRESS.
			return result;
		}
		//nobreak;
	case ESP82_State2:
		// AT+CIPSTART
		return ESP82_execute(ESP82_cmdBuffer, ESP82_RES_OK, ESP82_TIMEOUT_MS_HOST_CONNECT, NULL, 0);
	}
}

/*
 * @brief Disconnects from server.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
ESP82_Result_t ESP82_CloseTCP(void) {
	return ESP82_execute("AT+CIPCLOSE\r\n", ESP82_RES_OK, ESP82_TIMEOUT_MS_CMD, NULL, 0);
}

/*
 * @brief Send data to server.
 * @param data Pointer to data buffer.
 * @param dataLength Size of data to send.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
ESP82_Result_t ESP82_Send(const char * const data, const uint8_t dataLength) {
	// Construct the command on entry.
	if(!ESP82_inProgress || (ESP82_SR_State != ESP82_Send)){
		// Set SR_State as Send.
		ESP82_SR_State = ESP82_Send;

		// Create the command.
		sprintf(ESP82_cmdBuffer, "AT+CIPSEND=%i\r\n", dataLength);
		ESP82_sendCmd(ESP82_cmdBuffer, strlen(ESP82_cmdBuffer), true);
	}

	// Send the data.
	return ESP82_sendData(data, dataLength);
}

/*
 * @brief Receive data from server.
 * @param data Pointer to data buffer.
 * @param dataLengthMax Size of the buffer.
 * @return SUCCESS, INPROGRESS or ERROR.
 */
ESP82_Result_t ESP82_Receive(char * const data, const uint8_t dataLengthMax) {
	static uint8_t internalState;
	static unsigned int expectedLength;
	char * terminatorPosition;

	// Set SR_State as Reveive.
	if(ESP82_SR_State != ESP82_Receive){
		ESP82_SR_State = ESP82_Receive;
		ESP82_inProgress = false;
	}

	// Receive the available data.
	// __HAL_LOCK(&hdma);
	int popLength = fifo_out(&rxFifo, &ESP82_resBuffer[ESP82_resBufferBack], ESP82_BUFFERSIZE_RESPONSE - 1 - ESP82_resBufferBack);
	ESP82_resBufferBack += popLength;
	uint8_t availableLength = (ESP82_resBufferBack - ESP82_resBufferFront);
	recv_end_flag == 0;

	// State machine.
	switch (internalState = (ESP82_inProgress ? internalState : ESP82_State0)) {
	case ESP82_State0:
		// Start timeout.
		ESP82_timeoutBegin();

		// In progress.
		ESP82_inProgress = true;

		// To the header-waiting state.
		internalState = ESP82_State1;

		//nobreak;
	case ESP82_State1:
		// Get the incoming data header.
		if(availableLength >= 7){
			// Check the header.
			if(0 == memcmp(&ESP82_resBuffer[ESP82_resBufferFront], "\r\n+IPD,", 7)){
				// Update the front pointer.
				ESP82_resBufferFront += 7;

				// Switch to next state and wait for length.
				internalState = ESP82_State2;
			}else{
				// Error occured, no +IPD header received.
				ESP82_inProgress = false;
				return ESP82_ERROR;
			}
		}
		else if(availableLength == 0){
			ESP82_inProgress = false;
			return ESP82_RECEIVE_NOTHING;
			// recv nothing;
		}
			

		break;
	case ESP82_State2:
		// Get the incoming data length.
		if(availableLength >= 2){
			// Check the length termination character.
			if(NULL != (terminatorPosition = memchr(&ESP82_resBuffer[ESP82_resBufferFront], ':', availableLength))){
				// Terminate the string and get the length.
				*terminatorPosition = '\0';
				expectedLength = atoi(&ESP82_resBuffer[ESP82_resBufferFront]);

				// Check if buffer is enough for the incoming data.
				if((expectedLength > dataLengthMax) || !expectedLength){
					// Error occured.
					ESP82_inProgress = false;
					return ESP82_ERROR;
				}

				// Update the front pointer.
				ESP82_resBufferFront = (terminatorPosition - ESP82_resBuffer) + 1;

				// Switch to next state and wait for length.
				internalState = ESP82_State3;
			}
		}

		break;
	case ESP82_State3:
		// Get data.
		if(availableLength >= expectedLength){
			// Check the length termination character.
			memcpy(data, &ESP82_resBuffer[ESP82_resBufferFront], expectedLength);
			ESP82_resBufferFront+= expectedLength;
			// Success, data received.
			ESP82_inProgress = false;
			return expectedLength;
		}

		break;
	default:
		internalState = ESP82_State0;
	}

	// Check for timeout.
	if(ESP82_timeoutIsExpired(ESP82_TIMEOUT_MS_RECEIVE)){
		// Fail.
		ESP82_inProgress = false;
		return ESP82_ERROR;
	}

	// In Progress.
	return ESP82_INPROGRESS;
}

void HAL_UART_IdleCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &huart2 && recv_end_flag == 1){
		fifo_in(&rxFifo, rxBuffer, rx_len);
		memset(rxBuffer+rx_len,0,RX_BUFFER_SIZE-rx_len);
		HAL_UART_Receive_DMA(&huart2, rxBuffer, RX_BUFFER_SIZE);
	}
}

