#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_DISABLE = 0,
    LED_IN,
    LED_OUT
} LEDShowTypeDef;

typedef enum {
    POWER_DISABLE = 0,
    POWER_IN,
    POWER_OUT
} PowerModeTypedef;

void app_init(void);
void PowerSwitchFunction(void);
void SensorReadFunction(void);
void DataReportFunction(void);
void IOUT_CHECKFunction(void);

#ifdef __cplusplus
}
#endif
#endif /*__ APP_H__ */
