#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/jugador.h"
#include "../include/tablero.h"
#include "../include/ipc.h"
#include "../include/visualizacion.h"

/* ── Hilo por ficha ── */
static void *hilo_ficha(void *arg)
{
    ArgsHilo *a = (ArgsHilo *)arg;
    /* El hilo simplemente existe; el movimiento lo coordina jugador_proceso */
    (void)a;
    return NULL;
}

/* ── Estrategia: elige qué ficha mover ── */
int jugador_elegir_ficha(Tablero *t, int jugador_id, int dado)
{
    /* Prioridad:
       1. Ficha en pasillo que puede avanzar sin pasarse
       2. Ficha en tablero con mayor avance
       3. Sacar ficha de la base si dado == 5
       4. -1 si no hay movimiento posible                */

    int mejor = -1;

    /* Revisar fichas en pasillo */
    for (int f = 0; f < NUM_FICHAS; f++) {
        Ficha *fi = &t->fichas[jugador_id][f];
        if (fi->estado == EN_PASILLO &&
            fi->posicion + dado < NUM_CASILLAS_PASILLO) {
            mejor = f;
        }
    }
    if (mejor >= 0) return mejor;

    /* Ficha en tablero con posición más avanzada */
    int max_pos = -1;
    for (int f = 0; f < NUM_FICHAS; f++) {
        Ficha *fi = &t->fichas[jugador_id][f];
        if (fi->estado == EN_TABLERO && fi->posicion > max_pos) {
            max_pos = fi->posicion;
            mejor   = f;
        }
    }
    if (mejor >= 0) return mejor;

    /* Sacar de base */
    if (dado == 5) {
        for (int f = 0; f < NUM_FICHAS; f++) {
            if (t->fichas[jugador_id][f].estado == EN_BASE)
                return f;
        }
    }

    return -1; /* sin movimiento */
}

void jugador_notificar_evento(int msqid, int jugador_origen,
                              int jugador_destino, int dato,
                              const char *texto)
{
    MensajeIPC msg;
    memset(&msg, 0, sizeof(msg));
    msg.tipo            = MSG_TIPO_EVENTO;
    msg.jugador_origen  = jugador_origen;
    msg.jugador_destino = jugador_destino;
    msg.dato            = dato;
    strncpy(msg.texto, texto, sizeof(msg.texto) - 1);
    msgq_enviar(msqid, &msg);
}

/* ── Proceso hijo ── */
void jugador_proceso(int jugador_id, Tablero *t, int socket_fd)
{
    /* Crear 4 hilos (uno por ficha) */
    pthread_t hilos[NUM_FICHAS];
    ArgsHilo  args[NUM_FICHAS];

    for (int f = 0; f < NUM_FICHAS; f++) {
        args[f].jugador_id = jugador_id;
        args[f].ficha_id   = f;
        args[f].tablero    = t;
        pthread_create(&hilos[f], NULL, hilo_ficha, &args[f]);
    }

    /* Bucle de turnos */
    while (1) {
        MensajeIPC msg;
        socket_recibir(socket_fd, &msg);

        if (msg.tipo == MSG_TIPO_FIN)
            break;

        if (msg.tipo == MSG_TIPO_TURNO) {
            t->stats[jugador_id].turnos_jugados++;

            int dado  = t->dado;
            int ficha = jugador_elegir_ficha(t, jugador_id, dado);

            if (ficha >= 0) {
                int movio = tablero_mover_ficha(t, jugador_id, ficha, dado);
                if (movio) {
                    vis_mostrar_evento("%s mueve ficha %d con dado %d",
                                      vis_nombre_jugador(jugador_id),
                                      ficha, dado);
                }
            } else {
                vis_mostrar_evento("%s no puede mover (dado=%d)",
                                   vis_nombre_jugador(jugador_id), dado);
            }

            /* Responder al árbitro: turno completado */
            MensajeIPC resp;
            memset(&resp, 0, sizeof(resp));
            resp.tipo           = MSG_TIPO_EVENTO;
            resp.jugador_origen = jugador_id;
            socket_enviar(socket_fd, &resp);
        }
    }

    /* Esperar a los hilos antes de salir */
    for (int f = 0; f < NUM_FICHAS; f++)
        pthread_join(hilos[f], NULL);
}
