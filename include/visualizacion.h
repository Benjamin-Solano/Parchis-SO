#ifndef VISUALIZACION_H
#define VISUALIZACION_H

#include "tipos.h"

/* Limpia la terminal, dibuja el tablero y muestra el último evento */
void vis_dibujar_tablero(const Tablero *t, const char *evento);

/* Muestra el marcador de estadísticas */
void vis_mostrar_stats(const Tablero *t);

/* Muestra el resultado final */
void vis_mostrar_ganador(int jugador_id);

/* Colores ANSI por jugador */
const char *vis_color_jugador(int jugador_id);
const char *vis_nombre_jugador(int jugador_id);

#define ANSI_RESET  "\033[0m"
#define ANSI_BOLD   "\033[1m"
#define ANSI_ROJO   "\033[31m"
#define ANSI_VERDE  "\033[32m"
#define ANSI_AZUL   "\033[34m"
#define ANSI_AMARILLO "\033[33m"

#endif /* VISUALIZACION_H */
