#include "UARTdriver.h"

typedef struct DriverHandel_UART
{
    volatile uint8_t _usageFlag;                  		//to flag that the strcture is valid (0 if not used)
    uint8_t _rxByte;                        			//where the HAL ISR will store the received byte
    UART_HandleTypeDef* _huartHandle;       			//UART handle
    IRQn_Type _irq;										//irq number of the uart
    QueueHandle_t	_rxQueueHandle;						//rx queue handle
    uint8_t _rxQueueStorageBuffer[SERIAL_RX_BUFF_LEN];	//rx queue data buffer
    StaticQueue_t _rxQueueBuffer;						//rx queue buffer
    QueueHandle_t	_txQueueHandle;						//tx queue handle
    uint8_t _txQueueStorageBuffer[SERIAL_TX_BUFF_LEN];	//tx queue data buffer
    StaticQueue_t _txQueueBuffer;						//tx queue buffer
    fifo_policy _policyRX;								//rx buffer policy
} DriverHandel_UART;

DriverHandel_UART _driverHandle_UART[MAX_UART_HANDLE]; 	//handle structures array

void initDriver_UART()
{
    //initializing the data structure
    for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
    {
        _driverHandle_UART[handleIndex]._usageFlag = 0;
    }
}

uint8_t addDriver_UART(UART_HandleTypeDef* huartHandle, IRQn_Type irq, fifo_policy policyRX)
{
    //scan the data structure to find a free position (or if the handle is already inserted)
    for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
    {
        //if it finds an occupied position
        if(_driverHandle_UART[handleIndex]._usageFlag == 1)
        {
            //if the handle is already inside the structure
            if(_driverHandle_UART[handleIndex]._huartHandle == huartHandle)
            {
                //error
                return 1; 
            }
        }
        else
        {
        	//disable the IRQ
        	uint32_t irqState=NVIC_GetEnableIRQ(irq);
        	NVIC_DisableIRQ(irq);

            //intialize the strcture for this handle
            _driverHandle_UART[handleIndex]._huartHandle = huartHandle;
            _driverHandle_UART[handleIndex]._rxQueueHandle = xQueueCreateStatic(SERIAL_RX_BUFF_LEN,1,(void*)&_driverHandle_UART[handleIndex]._rxQueueStorageBuffer,&_driverHandle_UART[handleIndex]._rxQueueBuffer);
            _driverHandle_UART[handleIndex]._txQueueHandle = xQueueCreateStatic(SERIAL_TX_BUFF_LEN,1,(void*)&_driverHandle_UART[handleIndex]._txQueueStorageBuffer,&_driverHandle_UART[handleIndex]._txQueueBuffer);
            _driverHandle_UART[handleIndex]._usageFlag = 1;
            _driverHandle_UART[handleIndex]._policyRX = policyRX;

            HAL_UART_Receive_IT(huartHandle,&_driverHandle_UART[handleIndex]._rxByte,1);

            if(irqState) NVIC_EnableIRQ(irq);

            return 0;
        }
    }
    return 1;
}

uint32_t receiveDriver_UART(UART_HandleTypeDef* huartHandle, uint8_t* buff, uint32_t size){

    if(size == 0) return 0;

    //scanning the array with the structures to find the handle
    for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
    {
        //if it finds the handle
        if((_driverHandle_UART[handleIndex]._usageFlag == 1) && (huartHandle == _driverHandle_UART[handleIndex]._huartHandle))
        {
        	uint8_t rxNum=0;
        	while(rxNum<size && xQueueReceive(_driverHandle_UART[handleIndex]._rxQueueHandle,&buff[rxNum],0)==pdTRUE){
        		rxNum++;
        	}

            //0 bytes read
            return rxNum;

            //in case reception crashed and error handle didn't relaunch it, relaunch it (this should not happen)
            //disable the IRQ
            //EDIT: removed part that disables interrupt to avoid losing packets if this function is
            //preempted, execution SHOULD be IRQ safe anyway
        	//uint32_t irqState=NVIC_GetEnableIRQ(_driverHandle_UART[handleIndex]._irq);
        	//NVIC_DisableIRQ(_driverHandle_UART[handleIndex]._irq);
            HAL_UART_Receive_IT(huartHandle,&_driverHandle_UART[handleIndex]._rxByte,1);
            //if(irqState) NVIC_EnableIRQ(_driverHandle_UART[handleIndex]._irq);

        }
    }
    return 0;
}

uint32_t sendDriver_UART(UART_HandleTypeDef* huartHandle,uint8_t* buff,uint32_t size){
	if(size == 0) return 0;

	//scanning the array with the structures to find the handle
	for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
	{
		//if it finds the handle
		if((_driverHandle_UART[handleIndex]._usageFlag == 1) && (huartHandle == _driverHandle_UART[handleIndex]._huartHandle))
		{
			//inserting bytes inside queue
			uint8_t txNum=0;
			while(txNum<size && xQueueSendToBack(_driverHandle_UART[handleIndex]._txQueueHandle,&buff[txNum],0)==pdTRUE){
				txNum++;
			}
			//if no transmission ongoing and pipe is not empty, start transmission now
            //disable the IRQ
        	uint32_t irqState=NVIC_GetEnableIRQ(_driverHandle_UART[handleIndex]._irq);
        	NVIC_DisableIRQ(_driverHandle_UART[handleIndex]._irq);

			uint8_t txByte;
			if(huartHandle->gState == HAL_UART_STATE_READY && xQueueReceive(_driverHandle_UART[handleIndex]._txQueueHandle,&txByte,0)){
				HAL_UART_Transmit_IT(_driverHandle_UART[handleIndex]._huartHandle, &txByte, 1); //try restarting transmit if not ongoing
			}

            if(irqState) NVIC_EnableIRQ(_driverHandle_UART[handleIndex]._irq);

			return txNum;
		}
	}

	return 0;
}

void flushRXDriver_UART(UART_HandleTypeDef* huartHandle){
	//scanning the structure array
	for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
	{
		//if it finds the handle in the structure
		if(_driverHandle_UART[handleIndex]._usageFlag == 1 && huartHandle == _driverHandle_UART[handleIndex]._huartHandle)
		{
			//flushing queue
			xQueueReset(_driverHandle_UART[handleIndex]._rxQueueHandle);
		}
	}
}

void flushTXDriver_UART(UART_HandleTypeDef* huartHandle){
	//scanning the structure array
	for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
	{
		//if it finds the handle in the structure
		if(_driverHandle_UART[handleIndex]._usageFlag == 1 && huartHandle == _driverHandle_UART[handleIndex]._huartHandle)
		{
			//flushing queue
			xQueueReset(_driverHandle_UART[handleIndex]._txQueueHandle);
		}
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huartHandle){
//scanning the array with the structures to find the handle

    for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
    {
        //if it finds the handle
        if((_driverHandle_UART[handleIndex]._usageFlag == 1) && (huartHandle == _driverHandle_UART[handleIndex]._huartHandle))
        {
            HAL_UART_Receive_IT(huartHandle,&_driverHandle_UART[handleIndex]._rxByte,1);
            return;
        }
    }
    return;
    
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	//scanning the structure array
    for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
    {
        //if it finds the handle in the structure
        if(_driverHandle_UART[handleIndex]._usageFlag == 1 && huart == _driverHandle_UART[handleIndex]._huartHandle)
        {
        	if(_driverHandle_UART[handleIndex]._policyRX==keep_new && xQueueIsQueueFullFromISR(_driverHandle_UART[handleIndex]._rxQueueHandle)){
        		uint8_t c;
        		xQueueReceiveFromISR(_driverHandle_UART[handleIndex]._rxQueueHandle, &c, NULL);
        	}

            xQueueSendToBackFromISR(_driverHandle_UART[handleIndex]._rxQueueHandle,(void*)&_driverHandle_UART[handleIndex]._rxByte,NULL);

            //relaunching ISR
            HAL_UART_Receive_IT(huart,&_driverHandle_UART[handleIndex]._rxByte,1);

            return;
        }
    }
	return;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	//scanning the structure array
    for(uint32_t handleIndex = 0; handleIndex < MAX_UART_HANDLE; handleIndex++)
    {
        //if it finds the handle in the structure
        if(_driverHandle_UART[handleIndex]._usageFlag == 1 && huart == _driverHandle_UART[handleIndex]._huartHandle)
        {
			uint8_t txByte;
			if(xQueueReceiveFromISR(_driverHandle_UART[handleIndex]._txQueueHandle,&txByte,NULL)==pdTRUE){
				HAL_UART_Transmit_IT(_driverHandle_UART[handleIndex]._huartHandle, &txByte, 1);
			}

            return;
        }
    }
	return;

}