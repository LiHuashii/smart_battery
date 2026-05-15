#ifndef __LED_H__
#define __LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void LEDControl(uint8_t num, uint8_t state);
void SetLedsByRSOC(uint8_t rsoc);

#ifdef __cplusplus
}
#endif
#endif /*__ LED_H__ */
