#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"
#include "ssd1306.h"

// === Definições para SSD1306 ===
ssd1306_t disp;

QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;

SemaphoreHandle_t xSemaphore_Trigger;

// == funcoes de inicializacao ===

void pin_callback(uint gpio, uint32_t events) {
    absolute_time_t absolute_time= get_absolute_time();
    xQueueSend(xQueueTime, &absolute_time, 0);
}



void trigger_task(){
    gpio_put(TRIGGER_PIN, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN, 0);
    xSemaphoreGive(xSemaphore_Trigger);
    vTaskDelay(150);
}

void echo_task(void* p){
    absolute_time_t start_time;
    absolute_time_t end_time;
    while(1){
        if (xQueueReceive(xQueueTime, &start_time, portMAX_DELAY)) {
        if (xQueueReceive(xQueueTime, &end_time, portMAX_DELAY)) {
            uint64_t pulse_duration_us = absolute_time_diff_us(start_time, end_time);
            float last_distance_cm = (float)pulse_duration_us *  0.0343 / 2.0f;
            xQueueSend(xQueueDistance, &last_distance_cm, 0);


        
        }
    }
}

    }

    

void oled_task(void* p){
    float distance_cm;
    char dist_str[20];

    while (1) {

        xSemaphoreTake(xSemaphore_Trigger, portMAX_DELAY);


        if (xQueueReceive(xQueueDistance, &distance_cm, pdMS_TO_TICKS(140))) {
            ssd1306_clear(&disp);
            sprintf(dist_str, "Dist: %.1f cm", distance_cm);
            ssd1306_draw_string(&disp, 0, 8, 2, dist_str);

            if (distance_cm <= 100.0 && distance_cm > 0) {
                gpio_put(LED_PIN_R, 1); gpio_put(LED_PIN_G, 0); gpio_put(LED_PIN_B, 1);
            } else {
                gpio_put(LED_PIN_R, 0); gpio_put(LED_PIN_G, 0); gpio_put(LED_PIN_B, 1);
            }

            int bar_width = (int)(distance_cm / 200.0f * 128);
            if (bar_width > 128) bar_width = 128;
            if (bar_width < 0) bar_width = 0;
            ssd1306_fill_rect(&disp, 0, 40, bar_width, 10, 1);
            ssd1306_show(&disp);
        }
        else {
            ssd1306_clear(&disp);
            ssd1306_draw_string(&disp, 0, 24, 2, "Falha no");
            ssd1306_draw_string(&disp, 0, 48, 2, "sensor!");
            ssd1306_show(&disp);
            gpio_put(LED_PIN_R, 0); gpio_put(LED_PIN_G, 1); gpio_put(LED_PIN_B, 1);
        }
    }
}
    



void oled_display_init(void) {
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);
    ssd1306_show(&disp);
}



void led_rgb_init(void) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 1);

    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);
    gpio_put(LED_PIN_G, 1);

    gpio_init(LED_PIN_B);
    gpio_set_dir(LED_PIN_B, GPIO_OUT);
    gpio_put(LED_PIN_B, 1);
}


int main() {
    stdio_init_all();
    led_rgb_init();
    oled_display_init();
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    xQueueDistance = xQueueCreate(10, sizeof(float));
    xQueueTime = xQueueCreate(10, sizeof(absolute_time_t));
    xSemaphore_Trigger = xSemaphoreCreateBinary();

    xTaskCreate(trigger_task, "Trigger Task", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 2048, NULL, 1, NULL);

    vTaskStartScheduler();


    while (true);
}
