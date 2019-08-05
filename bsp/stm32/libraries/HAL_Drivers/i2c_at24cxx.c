/*
 * Copyright (c) 2003-2019, RZDQ
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-08     HQ           first version
 * 2019-07-19     HQ           ����������֧�ֶ��ֽڶ�д
 * ˵����Ŀǰ������֧��AT24C02
 */
#include <rtthread.h>
#include <rtdevice.h>
#include "i2c_at24cxx.h"

#define at24c_I2CBUS_NAME  "i2c4"                  /* I2C�豸���� */
#define at24c_ADDR          0X50                   /* �豸��ַ */
static struct rt_i2c_bus_device *at24c_i2c_bus;    /* I2C�豸��� */

#if 1
#define MPUDEBUG      rt_kprintf
#else
#define MPUDEBUG(...)
#endif

#define AT24C02
#ifdef AT24C02
#define EE_MODEL_NAME		"AT24C02"
#define EE_PAGE_SIZE		8			/* ҳ���С(�ֽ�) */
#define EE_SIZE				8*32		/* ������(�ֽ�) */
#define EE_ADDR_BYTES		1			/* ��ַ�ֽڸ��� */
#endif

/*
 *������ at24c_write_reg
 *����˵������ָ����ַд��������
 *�β�   reg д���ָ����ַ
 *       len д������ݳ���
 *       buf Ҫд�뵽����ָ��
 */
rt_err_t at24c_write_reg(rt_uint8_t reg, rt_uint16_t len, rt_uint8_t *buf)
{
    /*
    д����EEPROM�������������������ȡ�ܶ��ֽڣ�ÿ��д����ֻ����ͬһ��page��
    ����24xx02��page size = 8
    �򵥵Ĵ�����Ϊ�����ֽ�д����ģʽ��ÿд1���ֽڣ������͵�ַ
    */

    rt_uint8_t write_data[2];
    rt_uint16_t i;
    for(i=0; i<len; i++)
    {
        write_data[0] = reg + i;
        write_data[1] = buf[i];
        struct rt_i2c_msg msgs;  
        msgs.addr = at24c_ADDR;             /* �ӻ���ַ */
        msgs.flags = RT_I2C_WR;             /* д��־ */
        msgs.buf   = write_data;            /* ��������ָ�� */
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
 *������ at24c_read_reg
 *����˵������ָ����ַ���������
 *�β�   reg �����ݵ�ָ����ַ
 *       len ��ȡ�����ݳ���
 *       buf �洢����������ָ��
 */
rt_err_t at24c_read_reg(rt_uint8_t reg, rt_uint16_t len, rt_uint8_t *buf)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = at24c_ADDR;    /* �ӻ���ַ */
    msgs[0].flags = RT_I2C_WR;       /* д��־ */
    msgs[0].buf   = &reg;            /* �ӻ��Ĵ�����ַ */
    msgs[0].len   = 1;               /* ���������ֽ��� */

    msgs[1].addr  = at24c_ADDR;    /* �ӻ���ַ */
    msgs[1].flags = RT_I2C_RD;       /* ����־ */
    msgs[1].buf   = buf;             /* ��ȡ����ָ�� */
    msgs[1].len   = len;             /* ��ȡ�����ֽ��� */

    if (rt_i2c_transfer(at24c_i2c_bus, msgs, 2) != 2)
    {
        MPUDEBUG("read EEPROM fail!\r\n");
        return -RT_ERROR;
    }
    else
        return RT_EOK;
}


//at24cӲ����ʼ��
//����ֵ: 0,�ɹ� / -1,�������
int at24c_hw_init(void)
{
    /* ����ϵͳ�е�iic�豸 */
    at24c_i2c_bus = rt_i2c_bus_device_find(at24c_I2CBUS_NAME);  /*����I2C�豸*/
    /* ���ҵ��豸����� */
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

    rt_kprintf("����eeprom��ɣ�\r\n");
}

static void read_eeprom_test(void)
{
    uint8_t buf[EE_SIZE];
	rt_uint16_t i;
	rt_int32_t iTime1, iTime2;

	/* ��EEPROM, ��ʼ��ַ = 0�� ���ݳ���Ϊ 256 */
	iTime1 = HAL_GetTick();	/* ���¿�ʼʱ�� */
	if (at24c_read_reg(0, EE_SIZE, (uint8_t *)buf) != RT_EOK)
	{
		rt_kprintf("��eeprom����\r\n");
	}
	else
	{
		iTime2 = HAL_GetTick();	/* ���½���ʱ�� */
		rt_kprintf("��eeprom�ɹ����������£�\r\n");
	}

	/* ��ӡ���� */
	for (i = 0; i < EE_SIZE; i++)
	{
		rt_kprintf(" %02X", buf[i]);

		if ((i & 31) == 31)
		{
			rt_kprintf("\r\n");	/* ÿ����ʾ16�ֽ����� */
		}
		else if ((i & 31) == 15)
		{
			rt_kprintf(" - ");
		}
	}

	/* ��ӡ���ٶ� */
	rt_kprintf("����ʱ: %dms, ���ٶ�: %dB/s\r\n", iTime2 - iTime1, (EE_SIZE * 1000) / (iTime2 - iTime1));
}

static void write_eeprom_test(void)
{
    uint8_t buf[EE_SIZE];
	uint16_t i;
	int32_t iTime1, iTime2;

	/* �����Ի����� */
	for (i = 0; i < EE_SIZE; i++)
	{
		buf[i] = i;
	}

	/* дEEPROM, ��ʼ��ַ = 0�����ݳ���Ϊ 256 */
	iTime1 = HAL_GetTick();	/* ���¿�ʼʱ�� */
	if (at24c_write_reg(0, EE_SIZE, buf) != RT_EOK)
	{
		rt_kprintf("дeeprom����\r\n");
		return;
	}
	else
	{
		iTime2 = HAL_GetTick();	/* ���½���ʱ�� */
		rt_kprintf("дeeprom�ɹ���\r\n");
	}

	/* ��ӡ���ٶ� */
	rt_kprintf("д��ʱ: %dms, д�ٶ�: %dB/s\r\n", iTime2 - iTime1, (EE_SIZE * 1000) / (iTime2 - iTime1));
}

MSH_CMD_EXPORT(erase_eeprom_test, erase_eeprom_test);
MSH_CMD_EXPORT(read_eeprom_test, read_eeprom_test);
MSH_CMD_EXPORT(write_eeprom_test, write_eeprom_test);
#endif // RT_USING_FINSH
