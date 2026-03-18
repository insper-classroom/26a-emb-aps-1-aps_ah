#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

#define BTN_VERMELHO 14
#define BTN_VERDE    13
#define BTN_AZUL     12
#define BTN_AMARELO  15

#define MAX_SEQ 100
#define TIMEOUT_US 3000000 // 3 segundos

#include "core1.h"

int seq[MAX_SEQ];
int tamanho = 1;

void init_btns(){
    int b[] = {BTN_VERMELHO, BTN_VERDE, BTN_AZUL, BTN_AMARELO};
    for(int i=0;i<4;i++){
        gpio_init(b[i]);
        gpio_set_dir(b[i], GPIO_IN);
        gpio_pull_up(b[i]);
    }
}

int read_btn(){
    if(!gpio_get(BTN_VERMELHO)) return 0;
    if(!gpio_get(BTN_VERDE))    return 1;
    if(!gpio_get(BTN_AZUL))     return 2;
    if(!gpio_get(BTN_AMARELO))  return 3;
    return -1;
}

void next(){
    seq[tamanho] = rand()%4;
}

void show(){
    for(int i=0;i<tamanho;i++){
        int c = seq[i];

        multicore_fifo_push_blocking((CMD_LED_ON<<24)|c);
        multicore_fifo_push_blocking((CMD_PLAY<<24)|c);

        sleep_ms(600);

        multicore_fifo_push_blocking((CMD_LED_OFF<<24)|c);

        sleep_ms(200);
    }
}

// retorna 1 = acerto, 0 = erro, -1 = timeout
int play(){
    for(int i=0;i<tamanho;i++){
        int b = -1;

        uint32_t inicio = time_us_32();

        while(b==-1){
            b = read_btn();
            if(time_us_32() - inicio > TIMEOUT_US){
                multicore_fifo_push_blocking((CMD_ERROR<<24));
                return -1;
            }
        }

        // debounce
        sleep_ms(50);
        while(read_btn() != -1);

        multicore_fifo_push_blocking((CMD_PLAY<<24)|b);

        if(b != seq[i]){
            multicore_fifo_push_blocking((CMD_ERROR<<24));
            return 0;
        }

        sleep_ms(300);
    }
    return 1;
}

int main(){
    stdio_init_all();
    init_btns();

    srand(time_us_32());

    multicore_launch_core1(core1_entry);

    seq[0] = rand()%4;

    while(1){
        printf("Rodada: %d\n", tamanho);

        show();

        int resultado = play();

        if(resultado == -1){
            printf("TIMEOUT! Pontuacao: %d\n", tamanho - 1);
            tamanho = 1;
            seq[0] = rand()%4;
            sleep_ms(2000);
            continue;
        }

        if(resultado == 0){
            printf("ERRO! Pontuacao: %d\n", tamanho - 1);
            tamanho = 1;
            seq[0] = rand()%4;
            sleep_ms(2000);
            continue;
        }

        tamanho++;
        next();

        sleep_ms(1000);
    }
}