#include "switch.h"
#include "led.h"
#include "cmsis_os.h"
#include "app.h"
#include "fh_log.h"
#include "bms.h"
#include "message.h"

extern uint8_t bms_tx_buffer[32];
extern uint16_t bms_tx_len;

extern BMS_BasicInfo_t bms_basicinfo;

LEDShowTypeDef led_show_type = LED_DISABLE;
PowerModeTypedef power_mode = POWER_DISABLE;

extern osThreadId_t SensorReadHandle;

extern osSemaphoreId_t PWR_KEYHandle;

void PowerOn(void){
	HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_SET); // 打开电源
	LEDControl(0,1);LEDControl(1,1);LEDControl(2,1);LEDControl(3,1);//第一次按下先点亮所有LED，提示用户进入开机流程

	BMS_MakeReadBasicInfoCmd(bms_tx_buffer, &bms_tx_len);
    BMS_SendData(bms_tx_buffer, bms_tx_len);
	HAL_Delay(20);
	BMS_MakeReadBasicInfoCmd(bms_tx_buffer, &bms_tx_len);
    BMS_SendData(bms_tx_buffer, bms_tx_len);

	if(HAL_GPIO_ReadPin(IN_Check_GPIO_Port, IN_Check_Pin) == GPIO_PIN_RESET) {//判断电池是否插入充电器，如果插入
		HAL_Delay(300);//等待300ms，防止抖动误判
		if(HAL_GPIO_ReadPin(IN_Check_GPIO_Port, IN_Check_Pin) == GPIO_PIN_RESET){//确定已经插入充电器 此时控制板电源已经打开
			// HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_RESET);
			// for(uint8_t i = 0;i < 2;i++){
			// 	BMS_MakeControlChargeMosCmd(true, bms_tx_buffer, &bms_tx_len);//打开充电开关
			// 	BMS_SendData(bms_tx_buffer, bms_tx_len);
			// 	HAL_Delay(50);
			// }
			led_show_type = LED_IN;//充电指示
			power_mode = POWER_IN;//充电模式
			return;
		}
	}

	// 等待第一次松开按钮
	while(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port, PWRCHECK_Pin) == GPIO_PIN_RESET) {
		HAL_Delay(1);
	}

	// 等待0.7秒内第二次按下
	uint32_t startTime = HAL_GetTick();
	while(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port, PWRCHECK_Pin) == GPIO_PIN_SET) {
		if(HAL_GetTick() - startTime > 700) {
			BMS_MakeReadBasicInfoCmd(bms_tx_buffer, &bms_tx_len);
    		BMS_SendData(bms_tx_buffer, bms_tx_len);
			LEDControl(0,0);LEDControl(1,0);LEDControl(2,0);LEDControl(3,0);
			HAL_Delay(300);
			SetLedsByRSOC(bms_basicinfo.rsoc);
			HAL_Delay(1500);
			HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_RESET);
			return;
		}
		HAL_Delay(1);
	}

	LEDControl(0,0);LEDControl(1,0);LEDControl(2,0);LEDControl(3,0);
	if(HAL_GPIO_ReadPin(OUT_Check_GPIO_Port, OUT_Check_Pin) == GPIO_PIN_SET) {//如果检测到电池没有插入，立即关闭电源
		HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_RESET);
		while(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port, PWRCHECK_Pin) == GPIO_PIN_RESET) {//等待电源按键完全松开
			HAL_Delay(1);
		}
		return;
	}

	LOGI("Waiting for power button press duration...");
	// 测量按下时间，达到2秒立即响应
	startTime = HAL_GetTick();
	while(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port, PWRCHECK_Pin) == GPIO_PIN_RESET) {
		uint32_t currentTime = HAL_GetTick();
		uint32_t pressTime = currentTime - startTime;

		// 根据按下的时长决定要点亮哪个 LED（每 500ms 点亮一个）
		int8_t target_index = -1;
		if (pressTime >= 1500) {
			target_index = 3;
		} else if (pressTime >= 1000) {
			target_index = 2;
		} else if (pressTime >= 500) {
			target_index = 1;
		} else if (pressTime >= 0) {
			target_index = 0;   // 刚按下时点亮第 0 个，方便演示（也可按需要调整）
		}
		LEDControl(target_index, 1);

		// 如果长按达到2秒，立即保持开机
		if(pressTime >= 1500) {
			//发送开机命令
			led_show_type = LED_OUT;//放电指示
			power_mode = POWER_OUT;//放电模式
			HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_SET);
			LOGI("powering on...");
			return; // 开机成功，退出函数
		}
		// LOGI("Power button pressed for %lu ms", pressTime);

		HAL_Delay(10);
	}

	// 如果执行到这里，说明按键在达到2秒前就松开了，关闭电源
	HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_RESET);
}

void PowerOff(void){
	led_show_type = LED_DISABLE;//停止刷新电量指示
	LEDControl(0,0);LEDControl(1,0);LEDControl(2,0);LEDControl(3,0);
	// 等待第一次松开按钮
	while(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port, PWRCHECK_Pin) == GPIO_PIN_RESET) {
		osDelay(1);
	}
    LEDControl(0,1);LEDControl(1,1);LEDControl(2,1);LEDControl(3,1);
	// 等待0.7秒内第二次按下
	uint32_t startTime = HAL_GetTick();
	while(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port, PWRCHECK_Pin) == GPIO_PIN_SET) {
			if(HAL_GetTick() - startTime > 700) {
                if(power_mode == POWER_IN){
                    led_show_type = LED_IN;//充电指示
                }else{
                    led_show_type = LED_OUT;//放电指示
                }
				return;
			}
			osDelay(1);
	}

	// 测量按下时间，达到2秒立即响应
	startTime = HAL_GetTick();
	while(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port, PWRCHECK_Pin) == GPIO_PIN_RESET) {
		uint32_t currentTime = HAL_GetTick();
		uint32_t pressTime = currentTime - startTime;

		int8_t target_off_index = -1;
		if (pressTime >= 1500) {
			target_off_index = 0;  // 熄灭 LED0
		} else if (pressTime >= 1000) {
			target_off_index = 1;  // 熄灭 LED1
		} else if (pressTime >= 500) {
			target_off_index = 2;  // 熄灭 LED2
		} else if (pressTime >= 0) {
			target_off_index = 3;  // 熄灭 LED3
		}
		LEDControl(target_off_index, 0);   // 熄灭 LED
		
		// 如果长按达到2秒，立即关机
		if(pressTime >= 1500) {
			//发送关机命令
			osThreadSuspend(SensorReadHandle);
			osDelay(50);
			BMS_MakeControlDischargeMosCmd(false, bms_tx_buffer, &bms_tx_len);
			BMS_SendData(bms_tx_buffer, bms_tx_len);
			osDelay(50);
			BMS_MakeControlChargeMosCmd(false, bms_tx_buffer, &bms_tx_len);
			BMS_SendData(bms_tx_buffer, bms_tx_len);
			osDelay(50);
            while(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port, PWRCHECK_Pin) == GPIO_PIN_RESET) {//等待电源按键完全松开
                osDelay(100);
            }
			HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_RESET);//关机
			return; // 关机成功，退出函数
		}
		osDelay(1);
	}
    if(power_mode == POWER_IN){
        led_show_type = LED_IN;//充电指示
    }else{
        led_show_type = LED_OUT;//放电指示
    }
	osSemaphoreAcquire(PWR_KEYHandle, 1);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
    if(GPIO_Pin==PWRCHECK_Pin) {
        if(HAL_GPIO_ReadPin(PWRCHECK_GPIO_Port,PWRCHECK_Pin) == GPIO_PIN_RESET)//开关按下
        {
            osSemaphoreRelease(PWR_KEYHandle);
        }
        __HAL_GPIO_EXTI_CLEAR_IT(PWRCHECK_Pin);
    }
}