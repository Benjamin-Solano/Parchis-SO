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

    while (1) {
        sem_wait(&a->sem_mover);
        if (a->terminado) break;

        a->resultado = tablero_mover_ficha(a->tablero, a->jugador_id,
                                           a->ficha_id, a->dado);
        if (a->resultado && a->msqid >= 0)
            jugador_notificar_evento(a->msqid, a->jugador_id, -1,
                                     a->dado, "movimiento");

        sem_post(&a->sem_listo);
    }
    return NULL;
}

/* ── Estrategia: elige qué ficha mover ── */
int jugador_elegir_ficha(Tablero *t, int jugador_id, int dado)
{
    /* Prioridad:
       1. Ficha en pasillo (cualquiera; tablero_mover_ficha gestiona el ingreso a meta)
       2. Ficha en tablero con mayor progreso relativo a la salida
       3. Sacar ficha de la base si dado == 5
       4. -1 si no hay movimiento posible                */

    int mejor = -1;

    /* 1. Fichas en pasillo — se acepta cualquier tirada, incluyendo las que llegan a meta */
    for (int f = 0; f < NUM_FICHAS; f++) {
        Ficha *fi = &t->fichas[jugador_id][f];
        if (fi->estado == EN_PASILLO) {
            mejor = f;
        }
    }
    if (mejor >= 0) return mejor;

    /* 2. Ficha en tablero con mayor progreso relativo a su casilla de salida */
    int salida       = tablero_pos_salida(jugador_id);
    int max_progreso = -1;
    for (int f = 0; f < NUM_FICHAS; f++) {
        Ficha *fi = &t->fichas[jugador_id][f];
        if (fi->estado == EN_TABLERO) {
            int progreso = (fi->posicion - salida + NUM_CASILLAS) % NUM_CASILLAS;
            if (progreso > max_progreso) {
                max_progreso = progreso;
                mejor        = f;
            }
        }
    }
    if (mejor >= 0) return mejor;

    /* 3. Sacar de base */
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
void jugador_proceso(int jugador_id, Tablero *t, int socket_fd, int msqid)
{
    pthread_t hilos[NUM_FICHAS];
    ArgsHilo  args[NUM_FICHAS];

    for (int f = 0; f < NUM_FICHAS; f++) {
        args[f].jugador_id = jugador_id;
        args[f].ficha_id   = f;
        args[f].tablero    = t;
        args[f].msqid      = msqid;
        args[f].dado       = 0;
        args[f].resultado  = 0;
        args[f].terminado  = 0;
        sem_init(&args[f].sem_mover, 0, 0);
        sem_init(&args[f].sem_listo, 0, 0);
        pthread_create(&hilos[f], NULL, hilo_ficha, &args[f]);
    }

    while (1) {
        MensajeIPC msg;
        socket_recibir(socket_fd, &msg);

        if (msg.tipo == MSG_TIPO_FIN)
            break;

        if (msg.tipo == MSG_TIPO_TURNO) {
            t->stats[jugador_id].turnos_jugados++;

            int dado          = t->dado;
            int comidas_antes = t->stats[jugador_id].fichas_comidas;
            int ficha         = jugador_elegir_ficha(t, jugador_id, dado);

            MensajeIPC resp;
            memset(&resp, 0, sizeof(resp));
            resp.tipo           = MSG_TIPO_EVENTO;
            resp.jugador_origen = jugador_id;

            if (ficha >= 0) {
                args[ficha].dado = dado;
                sem_post(&args[ficha].sem_mover);
                sem_wait(&args[ficha].sem_listo);

                if (args[ficha].resultado)
                    snprintf(resp.texto, sizeof(resp.texto),
                             "%s mueve ficha %d con dado %d",
                             vis_nombre_jugador(jugador_id), ficha, dado);
                else
                    snprintf(resp.texto, sizeof(resp.texto),
                             "%s no pudo mover ficha %d (dado=%d)",
                             vis_nombre_jugador(jugador_id), ficha, dado);
            } else {
                snprintf(resp.texto, sizeof(resp.texto),
                         "%s no puede mover (dado=%d)",
                         vis_nombre_jugador(jugador_id), dado);
            }

            /* Señalizar al árbitro si se comió una ficha rival */
            resp.dato = (t->stats[jugador_id].fichas_comidas > comidas_antes) ? 1 : 0;
            socket_enviar(socket_fd, &resp);
        }
    }

    /* Señalar a todos los hilos que deben terminar y esperarlos */
    for (int f = 0; f < NUM_FICHAS; f++) {
        args[f].terminado = 1;
        sem_post(&args[f].sem_mover);
    }
    for (int f = 0; f < NUM_FICHAS; f++) {
        pthread_join(hilos[f], NULL);
        sem_destroy(&args[f].sem_mover);
        sem_destroy(&args[f].sem_listo);
    }
}
