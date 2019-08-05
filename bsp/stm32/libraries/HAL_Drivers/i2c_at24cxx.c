/*
 * Copyright (c) 2003-2019, RZDQ
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-08     HQ           first version
 * 2019-07-19     HQ           更新驱动，支持多字节读写
 * 说明：目前驱动仅支持AT24C02
 */
#include <rtthread.h>
#include <rtdevice.h>
#include "i2c_at24cxx.h"

#define at24c_I2CBUS_NAME  "i2c4"                  /* I2C设备名称 */
#define at24c_ADDR          0X50                   /* 设备地址 */
static struct rt_i2c_bus_device *at24c_i2c_bus;    /* I2C设备句柄 */

#if 1
#define MPUDEBUG      rt_kprintf
#else
#define MPUDEBUG(...)
#endif

#define AT24C02
#ifdef AT24C02
#define EE_MODEL_NAME		"AT24C02"
#define EE_PAGE_SIZE		8			/* 页面大小(字节) */
#define EE_SIZE				8*32		/* 总容量(字节) */
#define EE_ADDR_BYTES		1			/* 地址字节个数 */
#endif

/*
 *函数名 at24c_write_reg
 *功能说明：向指定地址写入多个数据
 *形参   reg 写入的指定地址
 *       len 写入的数据长度
 *       buf 要写入到数据指针
 */
rt_err_t at24c_write_reg(rt_uint8_t reg, rt_uint16_t len, rt_uint8_t *buf)
{
    /*
    写串行EEPROM不像读操作可以连续读取很多字节，每次写操作只能在同一个page。
    对于24xx02，page size = 8
    简单的处理方法为：按字节写操作模式，每写1个字节，都发送地址
    */

    rt_uint8_t write_data[2];
    rt_uint16_t i;
    for(i=0; i<len; i++)
    {
        write_data[0] = reg + i;
        write_data[1] = buf[i];
        struct rt_i2c_msg msgs;  
        msgs.addr = at24c_ADDR;             /* 从机地址 */
        msgs.flags = RT_I2C_WR;             /* 写标志 */
        msgs.buf   = write_data;            /* 发送数据指针 */
        msgs.len   = 2;
        
        if (rt_i2c_transfer(at24c_i2c_bus, &msgs, 1) != 1)
        {
            MPUDEBUG("write EEPROM fail!\r\n");
            return -RT_ERROR;
        }
        rt_thread_mdelay(5);
    }
    return -RT_EOK;
    
}

/*
 *函数名 at24c_read_reg
 *功能说明：从指定地址读多个数据
 *形参   reg 读数据的指定地址
 *       len 读取的数据长度
 *       buf 存储读数的数据指针
 */
rt_err_t at24c_read_reg(rt_uint8_t reg, rt_uint16_t len, rt_uint8_t *buf)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = at24c_ADDR;    /* 从机地址 */
    msgs[0].flags = RT_I2C_WR;       /* 写标志 */
    msgs[0].buf   = &reg;            /* 从机寄存器地址 */
    msgs[0].len   = 1;               /* 发送数据字节数 */

    msgs[1].addr  = at24c_ADDR;    /* 从机地址 */
    msgs[1].flags = RT_I2C_RD;       /* 读标志 */
    msgs[1].buf   = buf;             /* 读取数据指针 */
    msgs[1].len   = len;             /* 读取数据字节数 */

    if (rt_i2c_transfer(at24c_i2c_bus, msgs, 2) != 2)
    {
        MPUDEBUG("read EEPROM fail!\r\n");
        return -RT_ERROR;
    }
    else
        return RT_EOK;
}


//at24c硬件初始化
//返回值: 0,成功 / -1,错误代码
int at24c_hw_init(void)
{
    /* 查找系统中的iic设备 */
    at24c_i2c_bus = rt_i2c_bus_device_find(at24c_I2CBUS_NAME);  /*查找I2C设备*/
    /* 查找到设备后将其打开 */
    if (at24c_i2c_bus == RT_NULL)
    {
        MPUDEBUG("can't find AT24C02 %s device\r\n", at24c_I2CBUS_NAME);
        return -RT_ERROR;
    }

    MPUDEBUG("EEPROM set i2c bus to %s\r\n", at24c_I2CBUS_NAME);

    return RT_EOK;
}
INIT_DEVICE_EXPORT(at24c_hw_init);


#ifdef RT_USING_FINSH
#include <finsh.h>
static void erase_eeprom_test(void)
{
	rt_uint16_t i;
    uint8_t buf[EE_SIZE];
    
	for (i = 0; i < EE_SIZE; i++)
	{
		buf[i] = 0xFF;
	}

	at24c_write_reg(0, EE_SIZE, buf);

    rt_kprintf("擦除eeprom完成！\r\n");
}

static void read_eeprom_test(void)
{
    uint8_t buf[EE_SIZE];
	rt_uint16_t i;
	rt_int32_t iTime1, iTime2;

	/* 读EEPROM, 起始地址 = 0， 数据长度为 256 */
	iTime1 = HAL_GetTick();	/* 记下开始时间 */
	if (at24c_read_reg(0, EE_SIZE, (uint8_t *)buf) != RT_EOK)
	{
		rt_kprintf("读eeprom出错！\r\n");
	}
	else
	{
		iTime2 = HAL_GetTick();	/* 记下结束时间 */
		rt_kprintf("读eeprom成功，数据如下：\r\n");
	}

	/* 打印数据 */
	for (i = 0; i < EE_SIZE; i++)
	{
		rt_kprintf(" %02X", buf[i]);

		if ((i & 31) == 31)
		{
			rt_kprintf("\r\n");	/* 每行显示16字节数据 */
		}
		else if ((i & 31) == 15)
		{
			rt_kprintf(" - ");
		}
	}

	/* 打印读速度 */
	rt_kprintf("读耗时: %dms, 读速度: %dB/s\r\n", iTime2 - iTime1, (EE_SIZE * 1000) / (iTime2 - iTime1));
}

static void write_eeprom_test(void)
{
    uint8_t buf[EE_SIZE];
	uint16_t i;
	int32_t iTime1, iTime2;

	/* 填充测试缓冲区 */
	for (i = 0; i < EE_SIZE; i++)
	{
		buf[i] = i;
	}

	/* 写EEPROM, 起始地址 = 0，数据长度为 256 */
	iTime1 = HAL_GetTick();	/* 记下开始时间 */
	if (at24c_write_reg(0, EE_SIZE, buf) != RT_EOK)
	{
		rt_kprintf("写eeprom出错！\r\n");
		return;
	}
	else
	{
		iTime2 = HAL_GetTick();	/* 记下结束时间 */
		rt_kprintf("写eeprom成功！\r\n");
	}

	/* 打印读速度 */
	rt_kprintf("写耗时: %dms, 写速度: %dB/s\r\n", iTime2 - iTime1, (EE_SIZE * 1000) / (iTime2 - iTime1));
}

MSH_CMD_EXPORT(erase_eeprom_test, erase_eeprom_test);
MSH_CMD_EXPORT(read_eeprom_test, read_eeprom_test);
MSH_CMD_EXPORT(write_eeprom_test, write_eeprom_test);
#endif // RT_USING_FINSH
