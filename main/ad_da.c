#include "include/ad_da.h"
#include "include/jbuf.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

//#define AUDIO_EXAMPLE_FILE

#ifdef AUDIO_EXAMPLE_FILE
#include "audio_example_file.h"
#define EXAMPLE_I2S_SAMPLE_RATE   (8000)
#else
#define EXAMPLE_I2S_SAMPLE_RATE   (16000)
#endif // AUDIO_EXAMPLE_FILE

static const char* TAG = "ad/da";

//i2s port number
#define EXAMPLE_I2S_NUM           (0)
//i2s data bits
#define EXAMPLE_I2S_SAMPLE_BITS   (16)
//I2S data format
#define EXAMPLE_I2S_FORMAT        (I2S_CHANNEL_FMT_ONLY_RIGHT) //(I2S_CHANNEL_FMT_RIGHT_LEFT)
//I2S channel number
#define EXAMPLE_I2S_CHANNEL_NUM   ((EXAMPLE_I2S_FORMAT < I2S_CHANNEL_FMT_ONLY_RIGHT) ? (2) : (1))

//I2S built-in ADC unit
#define I2S_ADC_UNIT              ADC_UNIT_1
//I2S built-in ADC channel
#define I2S_ADC_CHANNEL           ADC1_CHANNEL_0

static uint16_t uns_buf[JBUF_FRAME_SIZE];



/** @brief I2S ADC/DAC mode init.
 */
void ad_da_init(void)
{
    ESP_LOGI(TAG, "Initializing...");
    int i2s_num = EXAMPLE_I2S_NUM;
    i2s_config_t i2s_config =
    {
        .mode = I2S_MODE_MASTER | /* I2S_MODE_RX | */ I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN /* | I2S_MODE_ADC_BUILT_IN */,
        .sample_rate =  EXAMPLE_I2S_SAMPLE_RATE,
        .bits_per_sample = EXAMPLE_I2S_SAMPLE_BITS,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .channel_format = EXAMPLE_I2S_FORMAT,
        .intr_alloc_flags = 0,
        .dma_buf_count = 4, //2,
        .dma_buf_len = JBUF_FRAME_SIZE * sizeof(short), //1024,
        .use_apll = 1,
    };
    //install and start i2s driver
    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    //init DAC pad
    if (EXAMPLE_I2S_CHANNEL_NUM == 2)
    {
        i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
    }
    else
    {
        i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
    }
    ////init ADC pad
    //i2s_set_adc_mode(I2S_ADC_UNIT, I2S_ADC_CHANNEL);
}

void set_clock()
{
    ESP_LOGI(TAG, "set clock");
    i2s_set_clk(EXAMPLE_I2S_NUM, EXAMPLE_I2S_SAMPLE_RATE, EXAMPLE_I2S_SAMPLE_BITS, EXAMPLE_I2S_CHANNEL_NUM);
}

/** @brief Scale data to 16bit/32bit for I2S DMA output.
 *        DAC can only output 8bit data value.
 *        I2S DMA will still send 16 bit or 32bit data, the highest 8bit contains DAC data.
 */
int example_i2s_dac_data_scale(uint8_t* d_buff, uint8_t* s_buff, uint32_t len)
{
    uint32_t j = 0;
#if (EXAMPLE_I2S_SAMPLE_BITS == 16)
    for (int i = 0; i < len; i++)
    {
        d_buff[j++] = 0;
        d_buff[j++] = s_buff[i];
    }
    return (len * 2);
#else
    for (int i = 0; i < len; i++)
    {
        d_buff[j++] = 0;
        d_buff[j++] = 0;
        d_buff[j++] = 0;
        d_buff[j++] = s_buff[i];
    }
    return (len * 4);
#endif
}

void convert_s16_to_u16(short *buf, unsigned short *out, uint32_t len)
{
    for (int i=0; i<len; i++)
    {
        out[i] = (int32_t)buf[i] + 0x8000;
    }
}

void i2s_adc_dac_fn(void *arg)
{
    set_clock();
    while (1)
    {
        size_t bytes_written = 0;
#ifdef AUDIO_EXAMPLE_FILE
        static int buf_pos = 0;
        convert_s16_to_u16((short*)(audio_table + buf_pos), uns_buf, JBUF_FRAME_SIZE);
        i2s_write(EXAMPLE_I2S_NUM, uns_buf, JBUF_FRAME_SIZE * sizeof(short), &bytes_written, portMAX_DELAY);
        if (buf_pos + JBUF_FRAME_SIZE * sizeof(short) <= sizeof(audio_table))
        {
            buf_pos += (JBUF_FRAME_SIZE * sizeof(short));
        }
        else
        {
            buf_pos = 0;
        }
#else
        short* buf = jbuf_get();
        convert_s16_to_u16(buf, uns_buf, JBUF_FRAME_SIZE);
        i2s_write(EXAMPLE_I2S_NUM, uns_buf, JBUF_FRAME_SIZE * sizeof(short), &bytes_written, portMAX_DELAY);
        if (bytes_written != JBUF_FRAME_SIZE * sizeof(short)) {
            ESP_LOGI(TAG, "written %d", bytes_written);
        }
#endif // AUDIO_EXAMPLE_FILE
        //vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}





