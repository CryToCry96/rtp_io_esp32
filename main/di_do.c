#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "include/di_do.h"
#include "include/modbus_params.h"

#define IO_TAG "I/O"

static xQueueHandle gpio_event_queue = NULL;

#define RL1 GPIO_NUM_19
#define RL2 GPIO_NUM_21

//Modify bit n'th of number to x
static uint8_t modifyBit(uint8_t n, uint8_t number, uint8_t x)
{
    number ^= (-x ^ number) & (1UL << n);
    return number;
}

//GPIO interrupt
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_event_queue, &gpio_num, NULL);
}

void gpio_task(void* arg)
{
    uint32_t io_num;
    for(;;) {

        //Read input from interrupt
        if(xQueueReceive(gpio_event_queue, &io_num, portMAX_DELAY)) {
            vTaskDelay(10);
            int gpio_value = gpio_get_level(io_num);
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_value);\

            //Change coil value for modbus.
            //We need modifyBit to change one bit from modbus coils
            uint8_t num = coil_reg_params.coils_DI;
            switch(io_num)
            {
                case 13: num = modifyBit(0, num, gpio_value);break;
                case 14: num = modifyBit(1, num, gpio_value);break;
                case 15: num = modifyBit(2, num, gpio_value);break;
                case 16: num = modifyBit(3, num, gpio_value);break;
                case 17: num = modifyBit(4, num, gpio_value);break;
                case 18: num = modifyBit(5, num, gpio_value);break;
                default: break;
            }
            coil_reg_params.coils_DI = num;
            ESP_LOGI(IO_TAG, "Set data to: 0x%x", num);
            
        }
    }
}

//Set DO status, get value from modbus coil
void set_DO() {
    gpio_set_level(RL1, (uint32_t)((coil_reg_params.coils_DO >> 0) & 1));//first bit of modbus coil
    gpio_set_level(RL2, (uint32_t)((coil_reg_params.coils_DO >> 1) & 1));//second bit
    ESP_LOGI(IO_TAG, "RL1: %d and RL2: %d", gpio_get_level(RL1), gpio_get_level(RL2));
}

void io_init(){
    //Config 6 input
    ESP_LOGI(IO_TAG, "Setting I/O");
    gpio_config_t gpioConfig;
    memset(&gpioConfig, 0, sizeof(gpioConfig));
    gpioConfig.pin_bit_mask = GPIO_SEL_13 | GPIO_SEL_14 | GPIO_SEL_15 | GPIO_SEL_16 | GPIO_SEL_17 | GPIO_SEL_18;
    gpioConfig.mode = GPIO_MODE_INPUT;
    gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE; //Enable internal resistor pull-up
    gpioConfig.pull_down_en = GPIO_PULLUP_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&gpioConfig);

    //Config 2 output
    gpioConfig.pin_bit_mask = GPIO_SEL_19 | GPIO_SEL_21;
    gpioConfig.mode = GPIO_MODE_INPUT_OUTPUT;
    gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLUP_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&gpioConfig);

    //create a queue to handle gpio event from isr
    gpio_event_queue = xQueueCreate(10,sizeof(uint32_t));
    gpio_install_isr_service(0);
    
    //Enable interrupt for input pin, add handler for interrupt
    gpio_isr_handler_add(GPIO_NUM_13, gpio_isr_handler, (void*) GPIO_NUM_13);
    gpio_isr_handler_add(GPIO_NUM_14, gpio_isr_handler, (void*) GPIO_NUM_14);
    gpio_isr_handler_add(GPIO_NUM_15, gpio_isr_handler, (void*) GPIO_NUM_15);
    gpio_isr_handler_add(GPIO_NUM_16, gpio_isr_handler, (void*) GPIO_NUM_16);
    gpio_isr_handler_add(GPIO_NUM_17, gpio_isr_handler, (void*) GPIO_NUM_17);
    gpio_isr_handler_add(GPIO_NUM_18, gpio_isr_handler, (void*) GPIO_NUM_18);

    //gpio_intr_enable(GPIO_SEL_13 | GPIO_SEL_14 | GPIO_SEL_15 | GPIO_SEL_16 | GPIO_SEL_17 | GPIO_SEL_18);
    cpu_num = xPortGetCoreID(); //Get CPU core
    ESP_LOGI(IO_TAG, "Interrupt setting on CPU: %d", cpu_num);
}