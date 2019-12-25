/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "MQTTPacket.h"
#include "ESP8266Client.h"
#include "MQTTPacket.h"
#include "transport.h"
#include "networkwrapper.h"
#include "stdio.h"
#include "fifo.h"
#include "bh1750_i2c_drv.h"
#include "topic_name_helper.h"
#include "wifi_credentials.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//#define SERVER_ADDR ("129.226.168.220")
#define CONNECTION_KEEPALIVE_S 60UL
#define PUB_WAIT_TIMEOUT 200UL //ms
#define PUB_WAIT_TICK 5UL //ms
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
PUTCHAR_PROTO //重写fputc函数
{
	HAL_UART_Transmit(&huart1, (uint8_t*) &ch, 1, 0xffff);
	return ch;
}
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t rxBuffer[RX_BUFFER_SIZE] = { 0 };
uint8_t debugSentBuffer[RX_BUFFER_SIZE];
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim2;
int recv_end_flag = 0;
int rx_len = 0;
struct fifo rxFifo;
uint8_t dat[2] = { 0 };
int ledMode = 2;
int ledSwitch = 0;
int lightSensorValue = 0;
int ledStatus = 0;
int MQTT_connected = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void MqttHandlerTask();
int lightSensorLux();
void updateDeviceInfo();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
long unsigned int network_gettime_ms(void) {
	return (HAL_GetTick());
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */
	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_I2C2_Init();
	MX_DMA_Init();
	MX_USART1_UART_Init();

	MX_USART2_UART_Init();
	MX_TIM2_Init();
	/* USER CODE BEGIN 2 */
	// HAL_UART_Receive_IT(&huart5, (uint8_t *)rxBuffer, 8);
	HAL_TIM_Base_Start_IT(&htim2);
	fifo_alloc(&rxFifo, FIFO_BUFFER_SIZE);
	HAL_UART_Receive_DMA(&huart2, rxBuffer, RX_BUFFER_SIZE);
	__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	HAL_GPIO_WritePin(LED0_GPIO_Port, LED1_Pin, SET);
	MqttHandlerTask();
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */
void MqttHandlerTask() {
	/* USER CODE BEGIN MqttHandlerTask */

	unsigned char buffer[128];
	MQTTTransport transporter;
	int result;
	int length;
	int startTime = HAL_GetTick();
	// Transport layer uses the esp8266 networkwrapper.
	static transport_iofunctions_t iof = { network_send, network_recv };
	int transport_socket = transport_open(&iof);

	// State machine.
	int internalState = 0;
	while (true) {
		switch (internalState) {
		case 0: {
			MQTT_connected = 0;
			// Initialize the network and connect to
			network_init();
			if (network_connect(SERVER_ADDR, 1883, CONNECTION_KEEPALIVE_S,
			false) == 0) {
				// To the next state.
				internalState++;
			}
		}
			break;
		case 1: {
			MQTT_connected = 0;
			// Populate the transporter.
			transporter.sck = &transport_socket;
			transporter.getfn = transport_getdatanb;
			transporter.state = 0;

			// Populate the connect struct.
			MQTTPacket_connectData connectData =
			MQTTPacket_connectData_initializer;
			connectData.MQTTVersion = 3;
			connectData.clientID.cstring = "LightSensor";
			connectData.keepAliveInterval = CONNECTION_KEEPALIVE_S * 2;
			length = MQTTSerialize_connect(buffer, sizeof(buffer),
					&connectData);

			// Send CONNECT to the mqtt broker.
			if ((result = transport_sendPacketBuffer(transport_socket, buffer,
					length)) == length) {
				// To the next state.
				internalState++;
			} else {
				// Start over.
				internalState = 0;
			}
		}
			break;
		case 2: {
			// Wait for CONNACK response from the mqtt broker.
			MQTT_connected = 0;
			while (true) {
				// Wait until the transfer is done.
				if ((result = MQTTPacket_readnb(buffer, sizeof(buffer),
						&transporter)) == CONNACK) {
					// Check if the connection was accepted.
					unsigned char sessionPresent, connack_rc;
					if ((MQTTDeserialize_connack(&sessionPresent, &connack_rc,
							buffer, sizeof(buffer)) != 1)
							|| (connack_rc != 0)) {
						// Start over.
						internalState = 0;
						break;
					} else {
						// To the next state.
						internalState++;
						break;
					}
				} else if (result == -1) {
					// Start over.
					internalState = 0;
					break;
				}
			}
		}
			break;
		case 3: {
			MQTT_connected = 0;
			// Populate the publish message.
			MQTTString leds_TopicString = MQTTString_initializer;
			MQTTString mode_TopicString = MQTTString_initializer;
			leds_TopicString.cstring = "leds";
			mode_TopicString.cstring = "mode";
			MQTTString topicFilters[2] = { leds_TopicString, mode_TopicString };
			int rQos[2] = { 0 };
			// length = MQTTSerialize_publish(buffer, sizeof(buffer), 0, 1, 0, 0,
			//                                topicString, payload, (length = sprintf(payload, "%d", lightSensorLux())));
			length = MQTTSerialize_subscribe(buffer, sizeof(buffer), 0, 9527, 2,
					topicFilters, rQos);

			// Send SUBSCRIBE to the mqtt broker.
			if ((result = transport_sendPacketBuffer(transport_socket, buffer,
					length)) == length) {
				// To the next state.
				internalState++;
				break;
			} else {
				// Start over.
				internalState = 0;
			}
		}
			break;
		case 4: {
			MQTT_connected = 0;
			// Wait for SUBACK response from the mqtt broker.
			while (true) {
				memset(buffer, 0, sizeof(buffer));
				// Wait until the transfer is done.
				if ((result = MQTTPacket_readnb(buffer, sizeof(buffer),
						&transporter)) == SUBACK) {
					// Check if the connection was accepted.
					unsigned char sessionPresent;
					int maxcount;
					int qCount;
					int qArray[5];
					if ((MQTTDeserialize_suback(&sessionPresent, 1, &qCount,
							qArray, buffer, sizeof(buffer)) == 1)) {
						internalState++;

						break;
					} else {
						internalState = 0;

						break;
					}
				} else if (result == -1) {
					// Start over.
					internalState = 0;
					break;
				}
			}
		}
			break;

		case 5: {
			static int pub_state;
			MQTT_connected = 1;
			// Populate the publish message.
			unsigned char payload[16];
			int rQos[1] = { 0 };
			MQTTString topicString = MQTTString_initializer;

			// Polling publish
			switch (pub_state) {
			case 0: {
				pub_state++;
				topicString.cstring = "ledmode";
				length = MQTTSerialize_publish(buffer, sizeof(buffer), 0, 1, 0,
						0, topicString, payload,
						(length = sprintf(payload, "%d", ledMode)));
			}
				break;
			case 1: {
				pub_state++;
				topicString.cstring = "light";
				length = MQTTSerialize_publish(buffer, sizeof(buffer), 0, 1, 0,
						0, topicString, payload,
						(length = sprintf(payload, "%d", lightSensorValue)));
			}
				break;
			case 2: {
				pub_state=0;
				topicString.cstring = "ledh";
				length = MQTTSerialize_publish(buffer, sizeof(buffer), 0, 1, 0,
						0, topicString, payload,
						(length = sprintf(payload, "%d", ledStatus)));
			}
				break;
			default:
				pub_state = 0;
				break;
			}

			if (recv_end_flag == 1) {
				internalState++;
				break;
			}
			// Send PUBLISH to the mqtt broker.
			if ((result = transport_sendPacketBuffer(transport_socket, buffer,
					length)) == length) {
				int len = sprintf(debugSentBuffer, "Published.\r\n");
				HAL_UART_Transmit_DMA(&huart1, debugSentBuffer, len);
				internalState++;
			} else {
				// Start over.
				internalState = 0;
			}
		}
			break;
		case 6: {
			MQTT_connected = 1;
			startTime = HAL_GetTick();
			// Wait for CONNACK response from the mqtt broker.
			static unsigned char buf[128];
			while (true) {
				memset(buf, 0, sizeof(buf));
				result = MQTTPacket_readnb(buf, sizeof(buffer), &transporter);
				// Wait until the transfer is done.
				if (result == PUBLISH) {

					unsigned char dup;
					int buflen = sizeof(buf);
					int qos;
					unsigned char retained;
					unsigned short msgid;
					int payloadlen_in;
					unsigned char *payload_in;
					int rc;
					char topicName[16] = { 0 };
					char *tnStart, *tnEnd;
					MQTTString receivedTopic;

					if (1
							!= MQTTDeserialize_publish(&dup, &qos, &retained,
									&msgid, &receivedTopic, &payload_in,
									&payloadlen_in, buf, buflen)) {
						if (HAL_GetTick() - startTime > PUB_WAIT_TIMEOUT) {
							internalState--;
							break;
						}
					} else {

						memcpy(topicName, receivedTopic.lenstring.data,
								receivedTopic.lenstring.len);
						int topic = getTopicCode(topicName);
						switch (topic) {
						case LEDS_TOPIC: {
							ledSwitch = getPayLoadValue(payload_in);
							if (ledMode == 1)
								HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin,
										!ledSwitch);
						}
							break;
						case MODE_TOPIC: {
							ledMode = getPayLoadValue(payload_in);
						}
							break;

						default:
							break;
						}

						// To the next state.
						recv_end_flag = 0;
						internalState--;
						break;
					}
				} else if (result == -1) {
					// Start over.
					internalState = 0;
					break;
				}
				// result !=0 and timeout
				else if (HAL_GetTick() - startTime > PUB_WAIT_TIMEOUT) {
					recv_end_flag = 0;
					internalState--;
					break;
				}
				// result !=0 and not timeout
				else {
					// updateDeviceInfo();
					HAL_Delay(PUB_WAIT_TICK);
				}
			}
		}
			break;
		default:
			internalState = 0;
		}
	}
	/* USER CODE END MqttHandlerTask */
}

int lightSensorLux() {
	if (HAL_OK == BH1750_Send_Cmd(ONCE_L_MODE)) {
		if (HAL_OK == BH1750_Read_Dat(dat))
			return BH1750_Dat_To_Lux(dat);
		else
			return -1;
	}
}

void updateDeviceInfo() {
	ledStatus = HAL_GPIO_ReadPin(LED0_GPIO_Port, LED0_Pin);
	ledStatus = !ledStatus;
	lightSensorValue = lightSensorLux();
	if (ledMode == 2)      // Auto mode
			{
		HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, lightSensorValue >= 50);
	} else if (ledMode == 1) {      // Manual mode
		HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, !ledSwitch);
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM2) {
		updateDeviceInfo();
		if (MQTT_connected) {
			HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		} else {
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, SET);
		}
	}
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
