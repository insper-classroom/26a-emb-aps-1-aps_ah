#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include <stdbool.h>
#include <stdint.h>
#include "core1.h"

#define AUDIO_PIN 20

#define LED0 16
#define LED1 17
#define LED2 18
#define LED3 19

#define PINO_TRIGGER 21
#define PINO_ECHO    11

int leds[4] = {LED0, LED1, LED2, LED3};

volatile int tone = 0;
volatile int counter = 0;

volatile absolute_time_t instante_borda_subida;
volatile bool flag_captura_sucesso = false;

void start_sound();

volatile uint32_t duracao_pulso_us = 0;

void handler_irq_echo(uint gpio, uint32_t events){
    if(events & GPIO_IRQ_EDGE_RISE){
        instante_borda_subida = get_absolute_time();
    } else if(events & GPIO_IRQ_EDGE_FALL){
        absolute_time_t descida = get_absolute_time();
        duracao_pulso_us = absolute_time_diff_us(instante_borda_subida, descida);
        flag_captura_sucesso = true;
    }
}

uint32_t medir_distancia_cm(){
    flag_captura_sucesso = false;

    gpio_put(PINO_TRIGGER, 1);
    busy_wait_us_32(10);
    gpio_put(PINO_TRIGGER, 0);

    uint32_t inicio = time_us_32();
    while(!flag_captura_sucesso){
        if(time_us_32() - inicio > 30000) return 999;
    }

    return duracao_pulso_us / 58;
}

void aguardar_mao(){
    while(1){
        uint32_t dist = medir_distancia_cm();
        if(dist <= 3){
            start_sound();
            multicore_fifo_push_blocking(CMD_START_GAME);
            break;
        }
        sleep_ms(200);
    }
}

void pwm_handler(){
    pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
    if(tone == 0){
        pwm_set_gpio_level(AUDIO_PIN, 0);
        return;
    }
    pwm_set_gpio_level(AUDIO_PIN, (counter++ % tone));
}

void set_tone(int id){
    counter = 0;
    switch(id){
        case 0: tone = 50;  break;
        case 1: tone = 100; break;
        case 2: tone = 150; break;
        case 3: tone = 200; break;
    }
}

void error_sound(){
    for(int i=0;i<3;i++){
        tone = 25;
        sleep_ms(150);
        tone = 0;
        sleep_ms(100);
    }
}

void start_sound(){
    for(int i=0;i<4;i++){
        tone = 50 + i*50;
        sleep_ms(120);
    }
    tone = 0;
}

void core1_entry(){
    for(int i=0;i<4;i++){
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
    }

    gpio_init(PINO_TRIGGER);
    gpio_set_dir(PINO_TRIGGER, GPIO_OUT);
    gpio_put(PINO_TRIGGER, 0);

    gpio_init(PINO_ECHO);
    gpio_set_dir(PINO_ECHO, GPIO_IN);
    gpio_set_irq_enabled_with_callback(PINO_ECHO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &handler_irq_echo);

    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    int slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 8.0f);
    pwm_config_set_wrap(&config, 255);
    pwm_init(slice, &config, true);
    pwm_set_irq_enabled(slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_handler);
    irq_set_enabled(PWM_IRQ_WRAP, true);


    while(1){
        if(multicore_fifo_rvalid()){
            uint32_t cmd = multicore_fifo_pop_blocking();
            uint8_t tipo = cmd >> 24;
            uint8_t dado = cmd & 0xFF;

            if(tipo == CMD_PLAY){
                set_tone(dado);
                sleep_ms(300);
                tone = 0;
            }

            if(tipo == CMD_LED_ON){
                gpio_put(leds[dado], 1);
            }

            if(tipo == CMD_LED_OFF){
                gpio_put(leds[dado], 0);
            }

            if(tipo == CMD_ERROR){
                gpio_put(leds[0], 1);
                gpio_put(leds[1], 1);
                gpio_put(leds[2], 1);
                gpio_put(leds[3], 1);
                error_sound();
                gpio_put(leds[0], 0);
                gpio_put(leds[1], 0);
                gpio_put(leds[2], 0);
                gpio_put(leds[3], 0);
            }

            if(tipo == CMD_AGUARDAR_MAO){
                aguardar_mao();
            }
        }
    }
}