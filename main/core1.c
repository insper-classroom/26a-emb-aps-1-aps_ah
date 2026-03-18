#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include "core1.h"

#define AUDIO_PIN 20

#define LED0 16
#define LED1 17
#define LED2 18
#define LED3 19

int leds[4] = {LED0, LED1, LED2, LED3};

volatile int tone = 0;
volatile int counter = 0;

void pwm_handler() {
    pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
    if (tone == 0) {
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

void set_tone_duplo(int id1, int id2){
    counter = 0;
    int par = id1 * 4 + id2;
    switch(par){
        case 0*4+1: tone = 30;  break;
        case 0*4+2: tone = 40;  break;
        case 0*4+3: tone = 45;  break;
        case 1*4+2: tone = 60;  break;
        case 1*4+3: tone = 70;  break;
        case 2*4+3: tone = 80;  break;
        default:    tone = 35;  break;
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

void core1_entry(){
    for(int i=0;i<4;i++){
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
    }

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

            if(tipo == CMD_PLAY_DUPLO){
                uint8_t id1 = (dado >> 4) & 0xF;
                uint8_t id2 = dado & 0xF;
                set_tone_duplo(id1, id2);
                sleep_ms(400);
                tone = 0;
            }

            if(tipo == CMD_LED_ON){
                gpio_put(leds[dado], 1);
            }

            if(tipo == CMD_LED_OFF){
                gpio_put(leds[dado], 0);
            }

            if(tipo == CMD_ERROR){
                error_sound();
            }
        }
    }
}