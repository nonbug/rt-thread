#ifndef __SPI_AD7606_H
#define __SPI_AD7606_H

#include <drv_common.h>
#include <board.h>

void get_ad7606_data(void);
extern struct rt_messagequeue mq_ad7606;

#endif/* __SPI_AD7606_H */
