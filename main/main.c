#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

#define BTN_VERMELHO 14
#define BTN_VERDE    13
#define BTN_AZUL     12
#define BTN_AMARELO  15

#define MAX_SEQ 100
#define TIMEOUT_US 3000000

#include "core1.h"

int seq[MAX_SEQ];
int seq_duplo[MAX_SEQ][2];
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

int read_btns_mask(){
    int mask = 0;
    if(!gpio_get(BTN_VERMELHO)) mask |= (1<<0);
    if(!gpio_get(BTN_VERDE))    mask |= (1<<1);
    if(!gpio_get(BTN_AZUL))     mask |= (1<<2);
    if(!gpio_get(BTN_AMARELO))  mask |= (1<<3);
    return mask;
}

int read_dois_btns(int esperado1, int esperado2){
    uint32_t inicio = time_us_32();

    while(1){
        if(time_us_32() - inicio > TIMEOUT_US) return -1;

        int mask = read_btns_mask();

        if(__builtin_popcount(mask) >= 2){
            int esperado_mask = (1<<esperado1) | (1<<esperado2);
            if((mask & esperado_mask) == esperado_mask){
                sleep_ms(100);
                while(read_btns_mask() != 0);
                return 1;
            } else {
                sleep_ms(100);
                while(read_btns_mask() != 0);
                return 0;
            }
        }

        sleep_ms(10);
    }
}

void next_normal(){
    seq[tamanho] = rand()%4;
}

void next_duplo(){
    int a = rand()%4;
    int b;
    do { b = rand()%4; } while(b == a);
    if(a > b){ int t=a; a=b; b=t; }
    seq_duplo[tamanho][0] = a;
    seq_duplo[tamanho][1] = b;
}

void show(){
    for(int i=0;i<tamanho;i++){
        if(tamanho >= 5){
            int c1 = seq_duplo[i][0];
            int c2 = seq_duplo[i][1];

            multicore_fifo_push_blocking((CMD_LED_ON<<24)|c1);
            multicore_fifo_push_blocking((CMD_LED_ON<<24)|c2);

            sleep_ms(600);

            multicore_fifo_push_blocking((CMD_LED_OFF<<24)|c1);
            multicore_fifo_push_blocking((CMD_LED_OFF<<24)|c2);

            sleep_ms(200);
        } else {
            int c = seq[i];

            multicore_fifo_push_blocking((CMD_LED_ON<<24)|c);
            multicore_fifo_push_blocking((CMD_PLAY<<24)|c);

            sleep_ms(600);

            multicore_fifo_push_blocking((CMD_LED_OFF<<24)|c);

            sleep_ms(200);
        }
    }
}

int play(){
    for(int i=0;i<tamanho;i++){
        if(tamanho >= 5){
            int resultado = read_dois_btns(seq_duplo[i][0], seq_duplo[i][1]);
            if(resultado == -1){
                multicore_fifo_push_blocking((CMD_ERROR<<24));
                return -1;
            }
            if(resultado == 0){
                multicore_fifo_push_blocking((CMD_ERROR<<24));
                return 0;
            }
            uint8_t dado_duplo = (seq_duplo[i][0]<<4) | seq_duplo[i][1];
            multicore_fifo_push_blocking((CMD_PLAY_DUPLO<<24)|dado_duplo);
            sleep_ms(300);
        } else {
            int b = -1;
            uint32_t inicio = time_us_32();

            while(b==-1){
                b = read_btn();
                if(time_us_32() - inicio > TIMEOUT_US){
                    multicore_fifo_push_blocking((CMD_ERROR<<24));
                    return -1;
                }
            }

            sleep_ms(50);
            while(read_btn() != -1);

            multicore_fifo_push_blocking((CMD_PLAY<<24)|b);

            if(b != seq[i]){
                multicore_fifo_push_blocking((CMD_ERROR<<24));
                return 0;
            }

            sleep_ms(300);
        }
    }
    return 1;
}

int main(){
    stdio_init_all();
    init_btns();

    srand(time_us_32());

    multicore_launch_core1(core1_entry);

    seq[0] = rand()%4;

    int a = rand()%4;
    int b;
    do { b = rand()%4; } while(b == a);
    if(a > b){ int t=a; a=b; b=t; }
    seq_duplo[0][0] = a;
    seq_duplo[0][1] = b;

    while(1){
        printf("Rodada: %d\n", tamanho);
        if(tamanho >= 5) printf("MODO DIFICIL!\n");

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
        if(tamanho >= 5){
            next_duplo();
        } else {
            next_normal();
        }

        sleep_ms(1000);
    }
}