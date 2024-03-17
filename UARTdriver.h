#ifndef UARTDRIVER_H
#define UARTDRIVER_H

#include "FreeRTOS.h"
#include "queue.h"
#include "main.h"

/* Interrupt driver for UART usinf FreeRTOS queues */

//maximum number of uarts the driver can handle
#define MAX_UART_HANDLE 4
//buffer lengths (bytes)
#define SERIAL_RX_BUFF_LEN 256
#define SERIAL_TX_BUFF_LEN 1024

//init driver data structure
void initDriver_UART();

//policy to follow in case a new byte must be inserted in a full fifo
typedef enum{
	keep_old,	//keep older byte, discard new
	keep_new,	//remove older bytes and insert new
} fifo_policy;

//add an UART to the driver handlers
//you must pass the HAL uart handle pointer (huardHandle) and the irq number (irq)
//you must also pass a policy (policyRX) for RX queue, the TX queue will always be 
//treated with the keep_old policy
//returns 0 in case of success, 1 otherwise
//NB. this function is not thread safe and should be called by the same thread
uint8_t addDriver_UART(UART_HandleTypeDef* huartHandle, IRQn_Type irq, fifo_policy policyRX);

//function to receive an amout (size) of bytes (buff) from uart (huardHandle)
//returns the actual number of receviced bytes
uint32_t receiveDriver_UART(UART_HandleTypeDef* huartHandle, uint8_t* buff, uint32_t size);

//function to send an amout (size) of bytes (buff) to uart (huardHandle)
//returns the actual number of sent bytes
uint32_t sendDriver_UART(UART_HandleTypeDef* huardHandle,uint8_t* buff,uint32_t size);

//function to flush uart (huardHandle) RX buffer
void flushRXDriver_UART(UART_HandleTypeDef* huartHandle);

//function to flush uart (huardHandle) TX buffer
void flushTXDriver_UART(UART_HandleTypeDef* huartHandle);

#endif
