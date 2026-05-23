#ifndef JUGADOR_H
#define JUGADOR_H

#include "tipos.h"

/* Punto de entrada del proceso hijo (jugador) */
void jugador_proceso(int jugador_id, Tablero *t, int socket_fd);

/* Lógica de decisión: qué ficha mover dado un resultado de dado */
int  jugador_elegir_ficha(Tablero *t, int jugador_id, int dado);

/* Notifica eventos a otros jugadores vía cola de mensajes */
void jugador_notificar_evento(int msqid, int jugador_origen,
                              int jugador_destino, int dato,
                              const char *texto);

#endif /* JUGADOR_H */
