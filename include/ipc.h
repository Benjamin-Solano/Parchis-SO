#ifndef IPC_H
#define IPC_H

#include "tipos.h"

/* ── Sockets (árbitro <-> jugadores) ── */
int  socket_crear_par(int fds[2]);
void socket_enviar(int fd, const MensajeIPC *msg);
int  socket_recibir(int fd, MensajeIPC *msg);
void socket_cerrar(int fd);

/* ── Colas de mensajes (eventos entre jugadores) ── */
int  msgq_crear(void);
void msgq_enviar(int msqid, const MensajeIPC *msg);
int  msgq_recibir(int msqid, MensajeIPC *msg, long tipo);
void msgq_destruir(int msqid);

/* ── Pipes (padre <-> hijos, estadísticas) ── */
int  pipe_crear(int fds[2]);
void pipe_enviar_stats(int fd, int jugador_id, const EstadisticasJugador *stats);
int  pipe_recibir_stats(int fd, int *jugador_id, EstadisticasJugador *stats);

#endif /* IPC_H */
