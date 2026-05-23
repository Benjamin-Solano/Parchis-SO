#include <stdio.h>

#include "../include/visualizacion.h"

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

void vis_dibujar_tablero(const Tablero *t, const char *evento)
{
    printf("\033[2J\033[H");
    printf(ANSI_BOLD "══════════════════ PARCHÍS SO ══════════════════\n" ANSI_RESET);
    printf("Turno: %s%-9s" ANSI_RESET "  Dado: " ANSI_BOLD "%d" ANSI_RESET "\n\n",
           vis_color_jugador(t->turno_actual),
           vis_nombre_jugador(t->turno_actual),
           t->dado);

    for (int j = 0; j < NUM_JUGADORES; j++) {
        const char *c = vis_color_jugador(j);
        printf("  %s%-9s" ANSI_RESET, c, vis_nombre_jugador(j));
        for (int f = 0; f < NUM_FICHAS; f++) {
            const Ficha *fi = &t->fichas[j][f];
            switch (fi->estado) {
                case EN_BASE:    printf(" %s[ B ]%s", c, ANSI_RESET); break;
                case EN_TABLERO: printf(" %s[%3d]%s", c, fi->posicion, ANSI_RESET); break;
                case EN_PASILLO: printf(" %s[P%2d]%s", c, fi->posicion, ANSI_RESET); break;
                case EN_META:    printf(" %s[ M ]%s", c, ANSI_RESET); break;
            }
        }
        printf("  %d/4\n", t->stats[j].fichas_en_meta);
    }

    printf("\n  >> %s\n", (evento && evento[0]) ? evento : "-");
    printf(ANSI_BOLD "════════════════════════════════════════════════\n" ANSI_RESET);
    fflush(stdout);
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

void vis_mostrar_ganador(int jugador_id)
{
    printf("\n" ANSI_BOLD "¡¡ GANADOR: %s%s" ANSI_RESET ANSI_BOLD " !!\n" ANSI_RESET,
           vis_color_jugador(jugador_id),
           vis_nombre_jugador(jugador_id));
}
