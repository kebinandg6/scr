#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// PINES

// Filas
static const gpio_num_t fil_pins[8] = {
    5, 18, 19, 23, 21, 3, 1, 22
};

// Columnas rojo
static const gpio_num_t col_rojo[8] = {
    32, 33, 25, 26, 17, 16, 4, 0
};

// Columnas verde
static const int col_verde[8] = {
    -1, 14, 12, 13, 2, 15, 27, -1
};

// Botones
#define BTN_IZQ 36
#define BTN_DER 39

// VARIABLES DEL JUEGO

int ast_x = 0;
int ast_y = 7;

int jugador_x = 3;   

uint32_t tiempo_ast = 0;
uint32_t tiempo_dificultad = 0;
uint32_t tiempo_game_over = 0;
uint32_t tiempo_btn = 0;

int velocidad_ast = 300;

bool game_over = false;

#define GAME_OVER_TIME 2000
#define ACELERACION_TIME 3000
#define VELOCIDAD_MIN 80
#define DELAY_BTN 120

// FILTRO DE BOTONES

#define MUESTRAS_ESTABLES 4

int cont_izq = 0;
int cont_der = 0;
bool bloqueo_botones = false;

// FUNCIONES BASICAS


void apagar_rojo() {
    for (int i = 0; i < 8; i++) {
        gpio_set_level(col_rojo[i], 1);
    }
}

void apagar_verde() {
    for (int i = 0; i < 8; i++) {
        if (col_verde[i] != -1) {
            gpio_set_level((gpio_num_t)col_verde[i], 1);
        }
    }
}

void apagar_filas() {
    for (int i = 0; i < 8; i++) {
        gpio_set_level(fil_pins[i], 0);
    }
}

void apagar_todo() {
    apagar_rojo();
    apagar_verde();
    apagar_filas();
}

void prender_verde(int col) {
    if (col < 0 || col > 7) return;

    if (col_verde[col] != -1) {
        gpio_set_level((gpio_num_t)col_verde[col], 0);
    }
}

void prender_rojo(int col) {
    if (col < 0 || col > 7) return;
    gpio_set_level(col_rojo[col], 0);
}

// DIBUJO

void dibujar_asteroide_en_fila(int fila) {
    if (fila == ast_y) {
        prender_rojo(ast_x);
    }
}

void dibujar_jugador_fila0() {
    if (jugador_x == 0) {
        prender_rojo(0);
        prender_verde(1);
    }
    else if (jugador_x == 6) {
        prender_verde(6);
        prender_rojo(7);
    }
    else {
        prender_verde(jugador_x);
        prender_verde(jugador_x + 1);
    }
}

// REFRESCO DISPLAY

void refrescar_fila(int fila) {
    apagar_todo();

    gpio_set_level(fil_pins[fila], 1);

    dibujar_asteroide_en_fila(fila);

    if (fila == 0) {
        dibujar_jugador_fila0();
    }

    vTaskDelay(pdMS_TO_TICKS(1));
    apagar_todo();
}

void refresco_extra_jugador() {
    apagar_todo();

    gpio_set_level(fil_pins[0], 1);
    dibujar_jugador_fila0();

    vTaskDelay(pdMS_TO_TICKS(1));
    apagar_todo();
}

void refrescar_display() {
    for (int fila = 0; fila < 8; fila++) {
        refrescar_fila(fila);
    }

    refresco_extra_jugador();
    refresco_extra_jugador();
}

// GAME OVER ROJO

void prender_todo_rojo_una_pasada() {
    for (int fila = 0; fila < 8; fila++) {
        apagar_todo();

        gpio_set_level(fil_pins[fila], 1);

        for (int c = 0; c < 8; c++) {
            prender_rojo(c);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
        apagar_todo();
    }
}

void parpadeo_game_over(uint32_t ahora) {
    if (ahora - tiempo_game_over < GAME_OVER_TIME) {
        for (int k = 0; k < 20; k++) {
            prender_todo_rojo_una_pasada();
        }

        apagar_todo();
        vTaskDelay(pdMS_TO_TICKS(80));
    } else {
        game_over = false;
        jugador_x = 3;
        ast_y = 7;
        ast_x = rand() % 8;
        velocidad_ast = 300;
        tiempo_dificultad = ahora;
        apagar_todo();
    }
}

// COLISION

bool hay_colision() {
    if (ast_y != 0) {
        return false;
    }

    if (jugador_x == 0) {
        return (ast_x == 0 || ast_x == 1);
    }
    else if (jugador_x == 6) {
        return (ast_x == 6 || ast_x == 7);
    }
    else {
        return (ast_x == jugador_x || ast_x == jugador_x + 1);
    }
}

// BOTONES ROBUSTOS

void actualizar_botones(uint32_t ahora) {
    int izq = gpio_get_level(BTN_IZQ);
    int der = gpio_get_level(BTN_DER);
    if (ahora - tiempo_btn < DELAY_BTN) {
        return;
    }

    if (bloqueo_botones) {
        if (izq == 0 && der == 0) {
            bloqueo_botones = false;
            cont_izq = 0;
            cont_der = 0;
        }
        return;
    }

    if (izq == 1 && der == 1) {
        cont_izq = 0;
        cont_der = 0;
        return;
    }

    if (izq == 1 && der == 0) {
        cont_izq++;
        cont_der = 0;

        if (cont_izq >= MUESTRAS_ESTABLES) {
            if (jugador_x > 0) {
                jugador_x--;
            }
            bloqueo_botones = true;
            cont_izq = 0;
            cont_der = 0;
            tiempo_btn = ahora;
        }
        return;
    }

    // Solo derecha
    if (der == 1 && izq == 0) {
        cont_der++;
        cont_izq = 0;

        if (cont_der >= MUESTRAS_ESTABLES) {
            if (jugador_x < 6) {
                jugador_x++;
            }
            bloqueo_botones = true;
            cont_izq = 0;
            cont_der = 0;
            tiempo_btn = ahora;
        }
        return;
    }

    // Ninguno activo
    cont_izq = 0;
    cont_der = 0;
}

// MAIN

void app_main() {
    gpio_config_t out = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    for (int i = 0; i < 8; i++) {
        out.pin_bit_mask |= (1ULL << fil_pins[i]);
        out.pin_bit_mask |= (1ULL << col_rojo[i]);
    }

    for (int i = 0; i < 8; i++) {
        if (col_verde[i] != -1) {
            out.pin_bit_mask |= (1ULL << col_verde[i]);
        }
    }

    gpio_config(&out);

    gpio_config_t btn = {
        .pin_bit_mask = (1ULL << BTN_IZQ) | (1ULL << BTN_DER),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&btn);

    srand((unsigned int)esp_timer_get_time());

    uint32_t inicio = esp_timer_get_time() / 1000;
    tiempo_dificultad = inicio;

    apagar_todo();

    while (1) {
        uint32_t ahora = esp_timer_get_time() / 1000;
        if (game_over) {
            parpadeo_game_over(ahora);
            continue;
        }
        if (ahora - tiempo_dificultad >= ACELERACION_TIME) {
            tiempo_dificultad = ahora;
            if (velocidad_ast > VELOCIDAD_MIN) {
                velocidad_ast -= 25;
            }
        }

        if (ahora - tiempo_ast >= (uint32_t)velocidad_ast) {
            tiempo_ast = ahora;
            ast_y--;

            if (ast_y < 0) {
                ast_y = 7;
                ast_x = rand() % 8;
            }

            if (hay_colision()) {
                game_over = true;
                tiempo_game_over = ahora;
            }
        }

        actualizar_botones(ahora);
        refrescar_display();
    }
}