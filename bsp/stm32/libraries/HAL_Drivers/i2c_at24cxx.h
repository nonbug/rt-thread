#ifndef __I2C_AT24CXX_H
#define __I2C_AT24CXX_H

#include <drv_common.h>
#include <board.h>

rt_err_t at24c_write_reg(rt_uint8_t reg, rt_uint16_t len, rt_uint8_t *buf);
rt_err_t at24c_read_reg(rt_uint8_t reg, rt_uint16_t len, rt_uint8_t *buf);

int at24c_hw_init(void);

#endif/* __I2C_AT24CXX_H */
