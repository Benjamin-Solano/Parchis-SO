#include <stdio.h>

#include "../include/visualizacion.h"

/* ════════════════════════════════════════════════════════════════
   MAPA ASCII EN CRUZ (tablero de Parchís) con colores por jugador.
   La info de cada ficha (estado + posicion) ya vive en el tablero
   compartido; esto es solo capa de presentación. El árbitro es el
   ÚNICO proceso que llama a esta función, así que la salida a la
   terminal (recurso compartido) tiene un solo escritor: no hay
   intercalado y no se necesita lock sobre stdout.
   ════════════════════════════════════════════════════════════════ */

#define MAPA_H 18
#define MAPA_W 18

/* Pista: posición global 0..67 -> (fila, columna) en pantalla */
static const int RING_RC[NUM_CASILLAS][2] = {
    {0,8},{0,9},{0,10},{1,10},{2,10},{3,10},{4,10},{5,10},{6,10},{7,10},
    {7,11},{7,12},{7,13},{7,14},{7,15},{7,16},{7,17},{8,17},{9,17},{10,17},
    {10,16},{10,15},{10,14},{10,13},{10,12},{10,11},{10,10},{11,10},{12,10},{13,10},
    {14,10},{15,10},{16,10},{17,10},{17,9},{17,8},{17,7},{16,7},{15,7},{14,7},
    {13,7},{12,7},{11,7},{10,7},{10,6},{10,5},{10,4},{10,3},{10,2},{10,1},
    {10,0},{9,0},{8,0},{7,0},{7,1},{7,2},{7,3},{7,4},{7,5},{7,6},
    {7,7},{6,7},{5,7},{4,7},{3,7},{2,7},{1,7},{0,7}
};

/* Pasillos: posición de pasillo 0..6 -> (fila, columna) por jugador */
static const int PAS_RC[NUM_JUGADORES][NUM_CASILLAS_PASILLO][2] = {
    {{1,8},{2,8},{3,8},{4,8},{5,8},{6,8},{7,8}},      /* Rojo: arriba    */
    {{8,15},{8,14},{8,13},{8,12},{8,11},{8,10},{8,9}},/* Verde: derecha  */
    {{15,8},{14,8},{13,8},{12,8},{11,8},{10,8},{9,8}},/* Azul: abajo     */
    {{8,1},{8,2},{8,3},{8,4},{8,5},{8,6},{8,7}}       /* Amarillo: izq.  */
};

/* Rótulos de base (esquinas) y meta (centro) */
static const int BASE_RC[NUM_JUGADORES][2] = {{1,1},{1,14},{15,14},{15,1}};
static const int META_RC[2] = {8,8};
static const char INICIAL[NUM_JUGADORES] = {'r','v','a','m'}; /* minuscula: no choca con la Meta 'M' */

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
    char ch[MAPA_H][MAPA_W];
    int  own[MAPA_H][MAPA_W];   /* dueño del color: -1 = sin color, 0..3 = jugador */

    for (int r = 0; r < MAPA_H; r++)
        for (int c = 0; c < MAPA_W; c++) { ch[r][c] = ' '; own[r][c] = -1; }

    /* 1) Pista del anillo */
    for (int i = 0; i < NUM_CASILLAS; i++)
        ch[RING_RC[i][0]][RING_RC[i][1]] = '.';

    /* 2) Casillas de salida, coloreadas por jugador */
    const int salidas[NUM_JUGADORES] = {
        SALIDA_ROJO, SALIDA_VERDE, SALIDA_AZUL, SALIDA_AMARILLO
    };
    for (int j = 0; j < NUM_JUGADORES; j++) {
        int p = salidas[j];
        ch[RING_RC[p][0]][RING_RC[p][1]]  = '*';
        own[RING_RC[p][0]][RING_RC[p][1]] = j;
    }

    /* 3) Pasillos, coloreados por jugador */
    for (int j = 0; j < NUM_JUGADORES; j++)
        for (int k = 0; k < NUM_CASILLAS_PASILLO; k++) {
            ch[PAS_RC[j][k][0]][PAS_RC[j][k][1]]  = ':';
            own[PAS_RC[j][k][0]][PAS_RC[j][k][1]] = j;
        }

    /* 4) Meta (centro) */
    ch[META_RC[0]][META_RC[1]]  = 'M';
    own[META_RC[0]][META_RC[1]] = -1;

    /* 5) Fichas sobre el mapa + conteos de base/meta */
    int base_cnt[NUM_JUGADORES] = {0,0,0,0};
    int meta_cnt[NUM_JUGADORES] = {0,0,0,0};
    int tab_cnt[NUM_JUGADORES]  = {0,0,0,0};
    int pas_cnt[NUM_JUGADORES]  = {0,0,0,0};
    for (int j = 0; j < NUM_JUGADORES; j++) {
        for (int f = 0; f < NUM_FICHAS; f++) {
            const Ficha *fi = &t->fichas[j][f];
            int r = -1, c = -1;
            if (fi->estado == EN_TABLERO) {
                tab_cnt[j]++;
                r = RING_RC[fi->posicion][0]; c = RING_RC[fi->posicion][1];
            } else if (fi->estado == EN_PASILLO) {
                pas_cnt[j]++;
                int p = fi->posicion;
                if (p < 0) p = 0;
                if (p >= NUM_CASILLAS_PASILLO) p = NUM_CASILLAS_PASILLO - 1;
                r = PAS_RC[j][p][0]; c = PAS_RC[j][p][1];
            } else if (fi->estado == EN_BASE) {
                base_cnt[j]++;
            } else { /* EN_META */
                meta_cnt[j]++;
            }
            if (r >= 0) {
                /* Si ya hay otra ficha en la celda, marcar apilamiento */
                if (ch[r][c] >= '0' && ch[r][c] <= '3') ch[r][c] = '#';
                else                                    ch[r][c] = (char)('0' + f);
                own[r][c] = j;
            }
        }
    }

    /* 6) Rótulos de base: inicial + número de fichas en base */
    for (int j = 0; j < NUM_JUGADORES; j++) {
        int r = BASE_RC[j][0], c = BASE_RC[j][1];
        ch[r][c]   = INICIAL[j];      own[r][c]   = j;
        ch[r][c+1] = (char)('0' + base_cnt[j]); own[r][c+1] = j;
    }

    /* ── Dibujar ── */
    printf("\033[2J\033[H");
    printf(ANSI_BOLD "════════════ PARCHÍS SO — MAPA ════════════\n" ANSI_RESET);
    printf("Turno: %s%-9s" ANSI_RESET "  Dado: " ANSI_BOLD "%d" ANSI_RESET "\n",
           vis_color_jugador(t->turno_actual),
           vis_nombre_jugador(t->turno_actual), t->dado);

    for (int r = 0; r < MAPA_H; r++) {
        for (int c = 0; c < MAPA_W; c++) {
            if (own[r][c] >= 0)
                printf("%s%c" ANSI_RESET, vis_color_jugador(own[r][c]), ch[r][c]);
            else
                printf("%c", ch[r][c]);
        }
        printf("\n");
    }

    /* Leyenda compacta por jugador */
    printf("\n");
    for (int j = 0; j < NUM_JUGADORES; j++) {
        printf("  %s%-9s" ANSI_RESET " base:%d tablero:%d pasillo:%d meta:%d/4\n",
               vis_color_jugador(j), vis_nombre_jugador(j),
               base_cnt[j], tab_cnt[j], pas_cnt[j], meta_cnt[j]);
    }

    printf("\n  >> %s\n", (evento && evento[0]) ? evento : "-");
    printf(ANSI_BOLD "═══════════════════════════════════════════\n" ANSI_RESET);
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