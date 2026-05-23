#ifndef TABLERO_H
#define TABLERO_H

#include "tipos.h"

/* ── Memoria compartida ── */
Tablero *tablero_crear(void);
Tablero *tablero_obtener(int shmid);
void     tablero_destruir(Tablero *t, int shmid);

/* ── Inicialización ── */
void tablero_init(Tablero *t);
void tablero_init_fichas(Tablero *t);
void tablero_init_sync(Tablero *t);

/* ── Lógica del tablero ── */
int  tablero_casilla_libre(Tablero *t, int posicion);
int  tablero_casilla_segura(int posicion);
int  tablero_mover_ficha(Tablero *t, int jugador, int ficha, int pasos);
void tablero_comer_ficha(Tablero *t, int jugador_atacante, int posicion);
int  tablero_ficha_llego_meta(Tablero *t, int jugador, int ficha);
int  tablero_jugador_gano(Tablero *t, int jugador);

/* ── Posiciones de salida/pasillo ── */
int  tablero_pos_salida(int jugador);
int  tablero_pos_pasillo_entrada(int jugador);

#endif /* TABLERO_H */
