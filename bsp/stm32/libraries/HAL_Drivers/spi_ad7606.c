/*
 * Copyright (c) 2003-2019, RZDQ
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-07-04     HQ           first version
 * 说明：完整的一次AD7606读取操作耗时约40us，故定时读取的时间不能少于40us -> freq<25KHZ
 * 数据输出接在DOUTB引脚，所以输出的数据不是按照顺序，数组ad7606_data的数据顺序为V3,V4,V1,V2
 */

#include <rtthread.h>
#include <rtdevice.h>
#include "drv_spi.h"
#include "spi_ad7606.h"

/*定义7606通道数*/
#define ADC_CH_NUM 4

/*AD7606 GPIO Configuration
PA4      ------> SPI1_CS
PA5      ------> SPI1_CLK
PA6      ------> SPI1_MISO
PA7      ------> SPI1_MOSI
PA8      ------> ADC_BSY
PG2      ------> ADC_RST
PG3      ------> ADC_STBY
PH9      ------> ADC_RANGE
*/
#define ADC_CS_PIN          GPIO_PIN_4
#define ADC_CS_PORT         GPIOA
#define ADC_BSY_PIN         GPIO_PIN_8
#define ADC_BSY_PORT        GPIOA
#define ADC_RST_PIN         GPIO_PIN_2
#define ADC_RST_PORT        GPIOG
#define ADC_STBY_PIN        GPIO_PIN_3
#define ADC_STBY_PORT       GPIOG
#define ADC_RANGE_PIN       GPIO_PIN_9
#define ADC_RANGE_PORT      GPIOH
#define ADC_CONVST_PIN      GPIO_PIN_7
#define ADC_CONVST_PORT     GPIOA
#define HIGH                GPIO_PIN_SET
#define LOW                 GPIO_PIN_RESET

#define AD7606_BUS_NAME             "spi1"
#define AD7606_SPI_DEVICE_NAME      "spi10"
struct rt_spi_device *spi_dev_ad7606;     /* SPI 设备句柄 */

//#define AD7606_DEBUG
#ifdef  AD7606_DEBUG
#define AD7606_TTACE         rt_kprintf
#else
#define AD7606_TTACE(...)
#endif

struct rt_messagequeue mq_ad7606;
unsigned short msg_pool_ad7606[16*64+4*64];
rt_uint16_t ad7606_data[ADC_CH_NUM];
static rt_uint16_t spi_send_buf[ADC_CH_NUM];

/*配置7606的其他IO口，spi已由驱动框架配置*/
static void ad7606_GPIO_config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    /*ADC_CS_PIN -> output*/
    GPIO_InitStructure.Pin = ADC_CS_PIN;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStructure.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(ADC_CS_PORT, &GPIO_InitStructure);

    /*ADC_RST_PIN -> output*/
    GPIO_InitStructure.Pin = ADC_RST_PIN;
    HAL_GPIO_Init(ADC_RST_PORT, &GPIO_InitStructure);

    /*ADC_STBY_PIN -> output*/
    GPIO_InitStructure.Pin = ADC_STBY_PIN;
    HAL_GPIO_Init(ADC_STBY_PORT, &GPIO_InitStructure);

    /*ADC_RANGE_PIN -> output*/
    GPIO_InitStructure.Pin = ADC_RANGE_PIN;
    HAL_GPIO_Init(ADC_RANGE_PORT, &GPIO_InitStructure);
    
    /*ADC_CONVST_PIN -> output*/
    GPIO_InitStructure.Pin = ADC_CONVST_PIN;
    HAL_GPIO_Init(ADC_CONVST_PORT, &GPIO_InitStructure);

    /*ADC_BSY_PIN -> input*/
    GPIO_InitStructure.Pin = ADC_BSY_PIN;
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init( ADC_BSY_PORT, &GPIO_InitStructure);
}

static int ad7606_hw_intit(void)
{
    rt_uint8_t i;
    ad7606_GPIO_config();
    rt_hw_spi_device_attach(AD7606_BUS_NAME, AD7606_SPI_DEVICE_NAME, ADC_CS_PORT, ADC_CS_PIN);

    /* 查找 spi 设备获取设备句柄 */
    spi_dev_ad7606 = (struct rt_spi_device *)rt_device_find(AD7606_SPI_DEVICE_NAME);
    if (spi_dev_ad7606 == RT_NULL)
    {
        rt_kprintf("can't find ad7606");
        return -RT_ERROR;
    }

    /*配置使用到的spi总线->当使用串行读取 VDRIVE=3.3V fsck最大17mhz，这里配置为15mhz*/
    struct rt_spi_configuration spi_ad7606_config;
    spi_ad7606_config.data_width = 16;
    spi_ad7606_config.max_hz = 15*1000*1000;
    spi_ad7606_config.mode = RT_SPI_MASTER | RT_SPI_MODE_3 | RT_SPI_MSB;
    rt_spi_configure(spi_dev_ad7606, &spi_ad7606_config);

    /*config ad7606  ->  ±10V scale*/
    HAL_GPIO_WritePin(ADC_STBY_PORT,ADC_STBY_PIN,HIGH);         /*ADC STBY HIGH->UNSLEEP*/
    HAL_GPIO_WritePin(ADC_RANGE_PORT,ADC_RANGE_PIN,HIGH);       /*ADC RANGE HIGH->±10V*/
    HAL_GPIO_WritePin(ADC_CONVST_PORT,ADC_CONVST_PIN,LOW);      /*ADC CONVST LOW->STOP CONVST*/
    HAL_GPIO_WritePin(ADC_CS_PORT,ADC_CS_PIN,HIGH);             /*CS HIGH ->UNSELECT ADC*/

    HAL_GPIO_WritePin(ADC_RST_PORT,ADC_RST_PIN,HIGH);           /*RST HIGH for at least 50ns reset the chip*/
    HAL_GPIO_WritePin(ADC_RST_PORT,ADC_RST_PIN,LOW);

    rt_mq_init(&mq_ad7606,"mqAD",&msg_pool_ad7606,16*sizeof(unsigned short),sizeof(msg_pool_ad7606),RT_IPC_FLAG_FIFO);

    for(i=0; i<ADC_CH_NUM; i++)
        spi_send_buf[i] = 0;

    return RT_EOK;
}
INIT_DEVICE_EXPORT(ad7606_hw_intit);


void get_ad7606_data(void)
{
    rt_uint8_t ad7606_start = 0x01;
    
    struct rt_spi_message start_ad7606_msg, get_ad7606_msg;
    start_ad7606_msg.send_buf = &ad7606_start;
    start_ad7606_msg.recv_buf = RT_NULL;
    start_ad7606_msg.length = 1;
    start_ad7606_msg.cs_take = 0;
    start_ad7606_msg.cs_release = 0;
    start_ad7606_msg.next = &get_ad7606_msg;
    
    get_ad7606_msg.send_buf = spi_send_buf;
    get_ad7606_msg.recv_buf   = ad7606_data;
    get_ad7606_msg.length     = ADC_CH_NUM ;
    get_ad7606_msg.cs_take    = 1;
    get_ad7606_msg.cs_release = 1;
    get_ad7606_msg.next        = RT_NULL;

    rt_spi_transfer_message(spi_dev_ad7606, &start_ad7606_msg);

    AD7606_TTACE("GET DATA is:V1=%d,V2=%d,V3=%d,V4=%d\n", ad7606_data[2], ad7606_data[3],ad7606_data[0],ad7606_data[1]);
    rt_mq_send(&mq_ad7606,ad7606_data,sizeof(ad7606_data));
}

#ifdef RT_USING_FINSH
#include <finsh.h>
static void get_one_ad7606(void)
{
    rt_uint8_t i;
    get_ad7606_data();
    for(i=0;i<4;i++)
    {
        if(ad7606_data[i]>0x8000)  //负数
            rt_kprintf("data%d = -%dmV  ",i,(0xffff-ad7606_data[i])*20000/65536);
        else
            rt_kprintf("data%d = %dmV  ",i,ad7606_data[i]*20000/65536);
    }
    rt_kprintf("\n\r");
}
MSH_CMD_EXPORT(get_one_ad7606, get_one_ad7606);
#endif // RT_USING_FINSH

