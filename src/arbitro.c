#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include "../include/arbitro.h"
#include "../include/tablero.h"
#include "../include/ipc.h"
#include "../include/visualizacion.h"

#define QUANTUM_BASE       1
#define QUANTUM_PRIORIDAD  2
#define FICHAS_CERCA_META  4   /* casillas para considerarse "cerca" */

void arbitro_init(Arbitro *a)
{
    srand((unsigned)time(NULL));
    for (int i = 0; i < NUM_JUGADORES; i++) {
        a->orden[i]      = i;
        a->quantum[i]    = QUANTUM_BASE;
        a->socket_fds[i] = -1;
    }
    a->indice_actual = 0;
}

/* ── Round Robin con prioridades dinámicas opcionales ── */
int arbitro_siguiente_turno(Arbitro *a, Tablero *t)
{
    /* Ajustar quantum por proximidad a meta */
    for (int j = 0; j < NUM_JUGADORES; j++) {
        int cerca = 0;
        for (int f = 0; f < NUM_FICHAS; f++) {
            Ficha *fi = &t->fichas[j][f];
            if (fi->estado == EN_PASILLO &&
                fi->posicion >= NUM_CASILLAS_PASILLO - FICHAS_CERCA_META)
                cerca++;
        }
        a->quantum[j] = cerca > 0 ? QUANTUM_PRIORIDAD : QUANTUM_BASE;
    }

    int jugador = a->orden[a->indice_actual];
    a->indice_actual = (a->indice_actual + 1) % NUM_JUGADORES;
    return jugador;
}

int arbitro_lanzar_dado(void)
{
    return (rand() % 6) + 1;
}

void arbitro_enviar_turno(Arbitro *a, int jugador)
{
    MensajeIPC msg;
    memset(&msg, 0, sizeof(msg));
    msg.tipo             = MSG_TIPO_TURNO;
    msg.jugador_destino  = jugador;
    socket_enviar(a->socket_fds[jugador], &msg);
}

void arbitro_recibir_stats(Arbitro *a, Tablero *t)
{
    (void)a;
    (void)t;
    /* Las estadísticas llegan por pipes al finalizar — ver main.c */
}

void arbitro_loop(Arbitro *a, Tablero *t, pid_t pids[NUM_JUGADORES])
{
    (void)pids;

    while (!t->partida_terminada) {
        pthread_mutex_lock(&t->mutex_turno);
        int jugador = arbitro_siguiente_turno(a, t);
        int dado    = arbitro_lanzar_dado();

        pthread_mutex_lock(&t->mutex_dado);
        t->dado          = dado;
        t->turno_actual  = jugador;
        pthread_mutex_unlock(&t->mutex_dado);
        pthread_mutex_unlock(&t->mutex_turno);

        arbitro_enviar_turno(a, jugador);

        /* Esperar señal de "turno completado" del jugador */
        MensajeIPC respuesta;
        socket_recibir(a->socket_fds[jugador], &respuesta);

        vis_dibujar_tablero(t);

        /* Verificar ganador */
        for (int j = 0; j < NUM_JUGADORES; j++) {
            if (tablero_jugador_gano(t, j)) {
                t->ganador           = j;
                t->partida_terminada = 1;
                break;
            }
        }
    }

    /* Notificar fin a todos los jugadores */
    for (int j = 0; j < NUM_JUGADORES; j++) {
        MensajeIPC fin;
        memset(&fin, 0, sizeof(fin));
        fin.tipo = MSG_TIPO_FIN;
        socket_enviar(a->socket_fds[j], &fin);
    }
}
