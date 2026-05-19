/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for PowerSwitch */
osThreadId_t PowerSwitchHandle;
const osThreadAttr_t PowerSwitch_attributes = {
  .name = "PowerSwitch",
  .priority = (osPriority_t) osPriorityHigh3,
  .stack_size = 512 * 4
};
/* Definitions for SensorRead */
osThreadId_t SensorReadHandle;
const osThreadAttr_t SensorRead_attributes = {
  .name = "SensorRead",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for DataReport */
osThreadId_t DataReportHandle;
const osThreadAttr_t DataReport_attributes = {
  .name = "DataReport",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for IOUT_CHECK */
osThreadId_t IOUT_CHECKHandle;
const osThreadAttr_t IOUT_CHECK_attributes = {
  .name = "IOUT_CHECK",
  .priority = (osPriority_t) osPriorityHigh1,
  .stack_size = 256 * 4
};
/* Definitions for LED */
osThreadId_t LEDHandle;
const osThreadAttr_t LED_attributes = {
  .name = "LED",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for bms_send_buf */
osMessageQueueId_t bms_send_bufHandle;
const osMessageQueueAttr_t bms_send_buf_attributes = {
  .name = "bms_send_buf"
};
/* Definitions for PWR_KEY */
osSemaphoreId_t PWR_KEYHandle;
const osSemaphoreAttr_t PWR_KEY_attributes = {
  .name = "PWR_KEY"
};
/* Definitions for BMS_RX */
osSemaphoreId_t BMS_RXHandle;
const osSemaphoreAttr_t BMS_RX_attributes = {
  .name = "BMS_RX"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void PowerSwitchTask(void *argument);
void SensorReadTask(void *argument);
void DataReportTask(void *argument);
void IOUT_CHECKTask(void *argument);
void LEDTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  app_init();
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of PWR_KEY */
  PWR_KEYHandle = osSemaphoreNew(1, 0, &PWR_KEY_attributes);

  /* creation of BMS_RX */
  BMS_RXHandle = osSemaphoreNew(1, 0, &BMS_RX_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of bms_send_buf */
  bms_send_bufHandle = osMessageQueueNew (16, 32, &bms_send_buf_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of PowerSwitch */
  PowerSwitchHandle = osThreadNew(PowerSwitchTask, NULL, &PowerSwitch_attributes);

  /* creation of SensorRead */
  SensorReadHandle = osThreadNew(SensorReadTask, NULL, &SensorRead_attributes);

  /* creation of DataReport */
  DataReportHandle = osThreadNew(DataReportTask, NULL, &DataReport_attributes);

  /* creation of IOUT_CHECK */
  IOUT_CHECKHandle = osThreadNew(IOUT_CHECKTask, NULL, &IOUT_CHECK_attributes);

  /* creation of LED */
  LEDHandle = osThreadNew(LEDTask, NULL, &LED_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_PowerSwitchTask */
/**
  * @brief  Function implementing the PowerSwitch thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_PowerSwitchTask */
void PowerSwitchTask(void *argument)
{
  /* USER CODE BEGIN PowerSwitchTask */
  /* Infinite loop */
  PowerSwitchFunction();
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END PowerSwitchTask */
}

/* USER CODE BEGIN Header_SensorReadTask */
/**
* @brief Function implementing the SensorRead thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_SensorReadTask */
void SensorReadTask(void *argument)
{
  /* USER CODE BEGIN SensorReadTask */
  /* Infinite loop */
  SensorReadFunction();
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END SensorReadTask */
}

/* USER CODE BEGIN Header_DataReportTask */
/**
* @brief Function implementing the DataReport thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_DataReportTask */
void DataReportTask(void *argument)
{
  /* USER CODE BEGIN DataReportTask */
  /* Infinite loop */
  DataReportFunction();
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END DataReportTask */
}

/* USER CODE BEGIN Header_IOUT_CHECKTask */
/**
* @brief Function implementing the IOUT_CHECK thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_IOUT_CHECKTask */
void IOUT_CHECKTask(void *argument)
{
  /* USER CODE BEGIN IOUT_CHECKTask */
  /* Infinite loop */
  IOUT_CHECKFunction();
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END IOUT_CHECKTask */
}

/* USER CODE BEGIN Header_LEDTask */
/**
* @brief Function implementing the LED thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LEDTask */
void LEDTask(void *argument)
{
  /* USER CODE BEGIN LEDTask */
  /* Infinite loop */
  LEDFunction();
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END LEDTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

