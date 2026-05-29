#ifndef ARBITRO_H
#define ARBITRO_H

#include "tipos.h"

/* ── Scheduler Round Robin ── */
typedef struct {
    int  orden[NUM_JUGADORES];        /* orden de turnos */
    int  quantum[NUM_JUGADORES];
    int  indice_actual;
    int  socket_fds[NUM_JUGADORES];   /* sockets hacia cada jugador */
    int  msqid;                       /* cola de eventos: el arbitro es el CONSUMIDOR */
    long eventos_consumidos;          /* total de eventos drenados de la cola */
} Arbitro;

void arbitro_init(Arbitro *a);
int  arbitro_siguiente_turno(Arbitro *a, Tablero *t);
void arbitro_enviar_turno(Arbitro *a, int jugador);
void arbitro_recibir_stats(Arbitro *a, Tablero *t);
void arbitro_drenar_eventos(Arbitro *a);   /* consume la cola de mensajes (no bloqueante) */
void arbitro_loop(Arbitro *a, Tablero *t, pid_t pids[NUM_JUGADORES]);
int  arbitro_lanzar_dado(void);

#endif /* ARBITRO_H */