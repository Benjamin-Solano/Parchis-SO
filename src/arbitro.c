#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

#include "../include/arbitro.h"
#include "../include/tablero.h"
#include "../include/ipc.h"
#include "../include/visualizacion.h"

#define QUANTUM_BASE       1
#define QUANTUM_PRIORIDAD  2
#define FICHAS_CERCA_META  4

void arbitro_init(Arbitro *a)
{
    srand((unsigned)time(NULL));
    for (int i = 0; i < NUM_JUGADORES; i++) {
        a->orden[i]      = i;
        a->quantum[i]    = QUANTUM_BASE;
        a->socket_fds[i] = -1;
    }
    a->indice_actual      = 0;
    a->msqid              = -1;
    a->eventos_consumidos = 0;
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

void arbitro_drenar_eventos(Arbitro *a)
{
    if (a->msqid < 0) return;

    MensajeIPC ev;
    while (msgrcv(a->msqid, &ev,
                  sizeof(MensajeIPC) - sizeof(long),
                  0 /* cualquier tipo */, IPC_NOWAIT) >= 0) {
        a->eventos_consumidos++;
    }
}

void arbitro_loop(Arbitro *a, Tablero *t, pid_t pids[NUM_JUGADORES])
{
    (void)pids;

    while (!t->partida_terminada) {
        /* Seleccionar jugador y leer su quantum antes de soltar el mutex */
        pthread_mutex_lock(&t->mutex_turno);
        int jugador = arbitro_siguiente_turno(a, t);
        int q       = a->quantum[jugador];
        pthread_mutex_unlock(&t->mutex_turno);

        /* El jugador juega su quantum base; turnos_pendientes crece si saca 6 o come */
        int turnos_pendientes = q;
        while (turnos_pendientes > 0 && !t->partida_terminada) {
            turnos_pendientes--;

            int dado = arbitro_lanzar_dado();

            pthread_mutex_lock(&t->mutex_dado);
            t->dado         = dado;
            t->turno_actual = jugador;
            pthread_mutex_unlock(&t->mutex_dado);

            arbitro_enviar_turno(a, jugador);

            MensajeIPC respuesta;
            memset(&respuesta, 0, sizeof(respuesta));
            socket_recibir(a->socket_fds[jugador], &respuesta);

            /* Consumir los eventos que el movimiento haya generado en la cola */
            arbitro_drenar_eventos(a);

            vis_dibujar_tablero(t, respuesta.texto);

            for (int j = 0; j < NUM_JUGADORES; j++) {
                if (tablero_jugador_gano(t, j)) {
                    t->ganador           = j;
                    t->partida_terminada = 1;
                    break;
                }
            }

            /* Turno extra por sacar 6 o por comer ficha rival */
            if (!t->partida_terminada && (dado == 6 || respuesta.dato == 1))
                turnos_pendientes++;
        }
    }

    /* Drenado final por si quedaron eventos en vuelo */
    arbitro_drenar_eventos(a);

    /* Notificar fin a todos los jugadores */
    for (int j = 0; j < NUM_JUGADORES; j++) {
        MensajeIPC fin;
        memset(&fin, 0, sizeof(fin));
        fin.tipo = MSG_TIPO_FIN;
        socket_enviar(a->socket_fds[j], &fin);
    }

    printf("\nEventos IPC consumidos de la cola de mensajes: %ld\n",
           a->eventos_consumidos);
}