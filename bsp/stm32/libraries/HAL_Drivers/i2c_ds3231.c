/*
 * Copyright (c) 2003-2019, RZDQ
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-04-25     HQ           first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include "i2c_ds3231.h"

#define DS3231_I2C_BUS_NAME        "i2c4"  /* 传感器连接的I2C总线设备名称 */
#define DS3231_ADDR                0x68    /* 从机地址 */
#define TIME_REG                   0X00

static struct rt_i2c_bus_device *ds3231_i2c_bus = RT_NULL;     /* I2C总线设备句柄 */

static uint8_t RTC_Bcd2ToByte(uint8_t Value)
{
    uint8_t tmp = 0;
    tmp = ((uint8_t)(Value & (uint8_t)0xF0) >> (uint8_t)0x4) * 10;
    return (tmp + (Value & (uint8_t)0x0F));
}

static uint8_t RTC_ByteToBcd2(uint8_t Value)
{
    uint8_t bcdhigh = 0;

    while (Value >= 10)
    {
        bcdhigh++;
        Value -= 10;
    }
    return  ((uint8_t)(bcdhigh << 4) | Value);
}

static rt_err_t ds3231_write_reg(rt_uint8_t reg, rt_uint8_t len, rt_uint8_t *data)
{
    struct rt_i2c_msg msgs;
    rt_uint8_t buf[len + 1];

    buf[0] = reg;
    for(rt_uint8_t i = 0; i < len; i++)
        buf[i + 1] = data[i];

    msgs.addr = DS3231_ADDR;                /*从机地址*/
    msgs.flags = RT_I2C_WR;                 /*写标志*/
    msgs.buf = buf;
    msgs.len = len + 1;

    if(rt_i2c_transfer(ds3231_i2c_bus, &msgs, 1) == 1)
    {
        return RT_EOK;
    }
    else
    {
        return -RT_ERROR;
    }
}

static rt_err_t ds3231_read_reg(rt_uint8_t reg, rt_uint8_t len, rt_uint8_t *buf)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr = DS3231_ADDR;
    msgs[0].flags = RT_I2C_WR;                 /*写标志*/
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = DS3231_ADDR;
    msgs[1].flags = RT_I2C_RD;                 /*读标志*/
    msgs[1].buf = buf;
    msgs[1].len = len;

    if(rt_i2c_transfer(ds3231_i2c_bus, msgs, 2) == 2)
    {
        return RT_EOK;
    }
    else
    {
        return -RT_ERROR;
    }
}

unsigned char rtc_Getting(struct tm *ti)
{
    unsigned char temp[7];

    ds3231_read_reg(TIME_REG, 7, temp);

    ti->tm_sec = (uint8_t)RTC_Bcd2ToByte(temp[0]);
    ti->tm_min = (uint8_t)RTC_Bcd2ToByte(temp[1]);
    ti->tm_hour = (uint8_t)RTC_Bcd2ToByte(temp[2]);
    ti->tm_mday = (uint8_t)RTC_Bcd2ToByte(temp[4]);
    ti->tm_mon = (uint8_t)RTC_Bcd2ToByte(temp[5]);
    ti->tm_year = (uint8_t)RTC_Bcd2ToByte(temp[6]) + 1970;

    return RT_EOK;
}

unsigned char rtc_Setting(struct tm to)
{
    rt_uint8_t buf[8];

    buf[0] = RTC_ByteToBcd2(to.tm_sec);
    buf[1] = RTC_ByteToBcd2(to.tm_min);
    buf[2] = RTC_ByteToBcd2(to.tm_hour);
    buf[4] = RTC_ByteToBcd2(to.tm_mday);
    buf[5] = RTC_ByteToBcd2(to.tm_mon);
    buf[6] = RTC_ByteToBcd2(to.tm_year - 1970);

    ds3231_write_reg(TIME_REG, 7, buf);

    return  RT_EOK;
}

int ds3231_hw_intit(void)
{  
    ds3231_i2c_bus = rt_i2c_bus_device_find(DS3231_I2C_BUS_NAME);/* 查找I2C总线设备，获取I2C总线设备句柄 */

    if (ds3231_i2c_bus == RT_NULL)
    {
        rt_kprintf("can't find ds3231");
        return RT_ERROR;
    }
    return RT_EOK;
}
INIT_DEVICE_EXPORT(ds3231_hw_intit);


#ifdef RT_USING_FINSH

#include <finsh.h>

void set_time(rt_uint8_t year, rt_uint8_t month, rt_uint8_t day, rt_uint8_t hour, rt_uint8_t minute, rt_uint8_t second)
{
    struct tm ti;

    ti.tm_year = year;
    ti.tm_mon = month;
    ti.tm_mday = day;
    ti.tm_hour = hour;
    ti.tm_min 	= minute;
    ti.tm_sec 	= second;
    if(rtc_Setting(ti) == RT_EOK)
        rt_kprintf("Setting Time success!\n");
    else
        rt_kprintf("Setting Time error!\n");

    if(rtc_Getting(&ti) == RT_EOK)
        rt_kprintf("%d-%d-%d %d:%d:%d\n", ti.tm_year, ti.tm_mon, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
    else
        rt_kprintf("Time I/O Error!\n");
}

FINSH_FUNCTION_EXPORT(set_time, set Time );
MSH_CMD_EXPORT(set_time, set Time);

static void Get_Time(void)
{
    struct tm ti;
    if(rtc_Getting(&ti) == RT_EOK)
        rt_kprintf("%d-%d-%d %d:%d:%d\n", ti.tm_year, ti.tm_mon, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
    else
        rt_kprintf("Time I/O Error!\n");
}
FINSH_FUNCTION_EXPORT(Get_Time, Get Time );
MSH_CMD_EXPORT(Get_Time, Get Time);

#endif
