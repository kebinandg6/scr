#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// Filas
static const gpio_num_t fil_pins[8] = {
    16, 21, 13, 18, 32, 12, 33, 27
};

// Columnas 
static const gpio_num_t col_pins[8] = {
    19, 25, 26, 17, 14, 5, 22, 23
};

// Botones
#define BTN_IZQ 15
#define BTN_DER 2

// Variables
int jugador_x = 3;
int ast_x = 3;
int ast_y = 7;

bool game_over = false;
uint32_t tiempo_game_over = 0;

uint32_t tiempo_ast = 0;
uint32_t tiempo_btn = 0;
uint32_t tiempo_dificultad = 0;

int velocidad_ast = 300; 

#define DELAY_BTN 200
#define GAME_OVER_TIME 2000

// Funciones
void apagar_columnas() {
    for (int i = 0; i < 8; i++) {
        gpio_set_level(col_pins[i], 0);
    }
}

void prender_todo() {
    for (int f = 0; f < 8; f++) {
        gpio_set_level(fil_pins[f], 0);
        for (int c = 0; c < 8; c++) {
            gpio_set_level(col_pins[c], 1);
        }
        esp_rom_delay_us(1000);
        gpio_set_level(fil_pins[f], 1);
    }
}

// Main
void app_main() {

    gpio_config_t out = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT
    };

    for (int i = 0; i < 8; i++) {
        out.pin_bit_mask |= (1ULL << fil_pins[i]);
        out.pin_bit_mask |= (1ULL << col_pins[i]);
    }
    gpio_config(&out);

    gpio_config_t btn = {
        .pin_bit_mask = (1ULL << BTN_IZQ) | (1ULL << BTN_DER),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE
    };
    gpio_config(&btn);

    srand(esp_timer_get_time());

    uint32_t inicio = esp_timer_get_time() / 1000;
    tiempo_dificultad = inicio;

    while (1) {

        uint32_t ahora = esp_timer_get_time() / 1000;

        // Dificultad aumentada cada 5 segundos
        if (ahora - tiempo_dificultad > 5000) {
            tiempo_dificultad = ahora;

            if (velocidad_ast > 80) { // Se agrega limite 
                velocidad_ast -= 30;
            }
        }

        // Perder
        if (game_over) {

            if (ahora - tiempo_game_over < GAME_OVER_TIME) {

                prender_todo();
                vTaskDelay(pdMS_TO_TICKS(150));

                for (int i = 0; i < 8; i++) {
                    gpio_set_level(fil_pins[i], 1);
                }
                apagar_columnas();
                vTaskDelay(pdMS_TO_TICKS(150));

            } else {
                game_over = false;
                jugador_x = 3;
                ast_y = 7;
                ast_x = rand() % 8;
                velocidad_ast = 300; // Velocidad asteroide 
            }

            continue;
        }

        // Asteroide
        if (ahora - tiempo_ast > velocidad_ast) {
            tiempo_ast = ahora;

            ast_y--;

            if (ast_y < 0) {
                ast_y = 7;
                ast_x = rand() % 8;
            }

            if (ast_y == 0) {
                if (ast_x == jugador_x || ast_x == jugador_x + 1) {
                    game_over = true;
                    tiempo_game_over = ahora;
                }
            }
        }

        // Botones
        if (ahora - tiempo_btn > DELAY_BTN) {

            if (gpio_get_level(BTN_DER)) {
                if (jugador_x < 6) jugador_x++;
                tiempo_btn = ahora;
            }
            else if (gpio_get_level(BTN_IZQ)) {
                if (jugador_x > 0) jugador_x--;
                tiempo_btn = ahora;
            }
        }

        // Display
        for (int fila = 0; fila < 8; fila++) {

            apagar_columnas();
            gpio_set_level(fil_pins[fila], 0);

            for (int col = 0; col < 8; col++) {

                bool encender = false;

                if (fila == ast_y && col == ast_x) encender = true;

                if (fila == 0) {
                    if (col == jugador_x || col == jugador_x + 1) {
                        encender = true;
                    }
                }

                gpio_set_level(col_pins[col], encender ? 1 : 0);
            }

            esp_rom_delay_us(1000);
            gpio_set_level(fil_pins[fila], 1);
        }
    }
}