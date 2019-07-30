#ifndef __I2C_DS3231_H
#define __I2C_DS3231_H

#include <drv_common.h>
#include <board.h>


unsigned char rtc_Getting(struct tm *ti);
unsigned char rtc_Setting(struct tm to);
    
#endif/* __I2C_DS3231_H */
