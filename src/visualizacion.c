#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../include/visualizacion.h"

/* ── Helpers ── */

const char *vis_color_jugador(int jugador_id)
{
    switch (jugador_id) {
        case ROJO:     return ANSI_ROJO;
        case VERDE:    return ANSI_VERDE;
        case AZUL:     return ANSI_AZUL;
        case AMARILLO: return ANSI_AMARILLO;
        default:       return ANSI_RESET;
    }
}

const char *vis_nombre_jugador(int jugador_id)
{
    switch (jugador_id) {
        case ROJO:     return "Rojo";
        case VERDE:    return "Verde";
        case AZUL:     return "Azul";
        case AMARILLO: return "Amarillo";
        default:       return "?";
    }
}

/* ── Símbolo de ficha ── */
static char simbolo_ficha(int jugador)
{
    return "RVAZ"[jugador];
}

/* ── Tablero ASCII ──
   Representación simplificada 11x11 del tablero de Parchís.
   Las 68 casillas del camino exterior se mapean en una cuadrícula.
   Las posiciones especiales (base, meta, pasillos) tienen celdas fijas.
*/

/* Mapa de posiciones del tablero al grid 15x15 */
#define ROWS 15
#define COLS 15

/* Devuelve el contenido visible de una celda del grid */
static void celda_contenido(const Tablero *t, int row, int col,
                             char *out, int out_size)
{
    /* TODO: mapeo completo de posiciones → celdas.
       Por ahora muestra un placeholder. */
    (void)t; (void)row; (void)col;
    snprintf(out, out_size, "   ");
}

void vis_dibujar_tablero(const Tablero *t)
{
    /* Limpiar terminal */
    printf("\033[2J\033[H");

    printf(ANSI_BOLD "══════════════════ PARCHÍS SO ══════════════════\n" ANSI_RESET);

    /* Cabecera de jugadores */
    for (int j = 0; j < NUM_JUGADORES; j++) {
        printf("%s%-10s" ANSI_RESET, vis_color_jugador(j), vis_nombre_jugador(j));
    }
    printf("\n");

    /* Contador de fichas por estado */
    for (int j = 0; j < NUM_JUGADORES; j++) {
        int base = 0, tablero_c = 0, pasillo = 0, meta = 0;
        for (int f = 0; f < NUM_FICHAS; f++) {
            switch (t->fichas[j][f].estado) {
                case EN_BASE:    base++;     break;
                case EN_TABLERO: tablero_c++;break;
                case EN_PASILLO: pasillo++;  break;
                case EN_META:    meta++;     break;
            }
        }
        printf("%sB:%d T:%d P:%d M:%d  " ANSI_RESET,
               vis_color_jugador(j), base, tablero_c, pasillo, meta);
    }
    printf("\n");

    /* Tablero visual simplificado */
    printf("\n");
    printf("  +");
    for (int c = 0; c < COLS; c++) printf("----+");
    printf("\n");

    for (int r = 0; r < ROWS; r++) {
        printf("  |");
        for (int c = 0; c < COLS; c++) {
            char buf[8];
            celda_contenido(t, r, c, buf, sizeof(buf));
            printf("%s|", buf);
        }
        printf("\n  +");
        for (int c = 0; c < COLS; c++) printf("----+");
        printf("\n");
    }

    /* Turno actual */
    printf("\nTurno: %s%s" ANSI_RESET "  |  Dado: %s%d" ANSI_RESET "\n",
           vis_color_jugador(t->turno_actual),
           vis_nombre_jugador(t->turno_actual),
           ANSI_BOLD, t->dado);
    printf("────────────────────────────────────────────────\n");
}

void vis_mostrar_stats(const Tablero *t)
{
    printf("\n" ANSI_BOLD "══════════ ESTADÍSTICAS FINALES ══════════\n" ANSI_RESET);
    printf("%-12s %8s %10s %10s %8s\n",
           "Jugador", "En meta", "Comidas", "Perdidas", "Turnos");
    printf("──────────────────────────────────────────\n");
    for (int j = 0; j < NUM_JUGADORES; j++) {
        const EstadisticasJugador *s = &t->stats[j];
        printf("%s%-12s" ANSI_RESET " %8d %10d %10d %8d\n",
               vis_color_jugador(j), vis_nombre_jugador(j),
               s->fichas_en_meta, s->fichas_comidas,
               s->fichas_perdidas, s->turnos_jugados);
    }
}

void vis_mostrar_evento(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("  >> ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

void vis_mostrar_ganador(int jugador_id)
{
    printf("\n" ANSI_BOLD "¡¡ GANADOR: %s%s" ANSI_RESET ANSI_BOLD " !!\n" ANSI_RESET,
           vis_color_jugador(jugador_id),
           vis_nombre_jugador(jugador_id));
}
