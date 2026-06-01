#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>

#include "../include/tablero.h"
#include "../include/tipos.h"

/* Casillas seguras del tablero estándar de Parchís */
const int CASILLAS_SEGURAS[NUM_CASILLAS_SEGURAS] = {
    5, 12, 17, 22, 29, 34, 39, 46, 51, 56, 63, 0,
    /* entradas a pasillos */
    4, 21, 38, 55
};


Tablero *tablero_crear(void)
{
    Tablero *t = mmap(NULL, sizeof(Tablero),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (t == MAP_FAILED)
        return NULL;
    return t;
}

Tablero *tablero_obtener(int shmid)
{
    (void)shmid;
    return NULL;
}

void tablero_destruir(Tablero *t, int shmid)
{
    (void)shmid;
    if (!t) return;

    for (int i = 0; i < NUM_CASILLAS; i++)
        pthread_mutex_destroy(&t->mutex_casillas[i]);

    for (int j = 0; j < NUM_JUGADORES; j++)
        for (int i = 0; i < NUM_CASILLAS_PASILLO; i++)
            pthread_mutex_destroy(&t->mutex_pasillos[j][i]);

    pthread_mutex_destroy(&t->mutex_dado);
    pthread_mutex_destroy(&t->mutex_turno);

    for (int j = 0; j < NUM_JUGADORES; j++) {
        sem_destroy(&t->sem_meta[j]);
        sem_destroy(&t->sem_pasillo[j]);
    }

    munmap(t, sizeof(Tablero));
}


void tablero_init(Tablero *t)
{
    memset(t, 0, sizeof(Tablero));

    for (int i = 0; i < NUM_CASILLAS; i++) {
        t->casillas[i].ocupante_jugador = -1;
        t->casillas[i].ocupante_ficha   = -1;
        t->casillas[i].es_segura        = 0;
    }

    for (int i = 0; i < NUM_CASILLAS_SEGURAS; i++) {
        int idx = CASILLAS_SEGURAS[i];
        if (idx >= 0 && idx < NUM_CASILLAS)
            t->casillas[idx].es_segura = 1;
    }

    for (int j = 0; j < NUM_JUGADORES; j++)
        for (int i = 0; i < NUM_CASILLAS_PASILLO; i++) {
            t->pasillos[j][i].ocupante_jugador = -1;
            t->pasillos[j][i].ocupante_ficha   = -1;
        }

    t->turno_actual      = ROJO;
    t->dado              = 0;
    t->partida_terminada = 0;
    t->ganador           = -1;

    tablero_init_fichas(t);
    tablero_init_sync(t);
}

void tablero_init_fichas(Tablero *t)
{
    for (int j = 0; j < NUM_JUGADORES; j++) {
        for (int f = 0; f < NUM_FICHAS; f++) {
            t->fichas[j][f].jugador_id = j;
            t->fichas[j][f].ficha_id   = f;
            t->fichas[j][f].estado     = EN_BASE;
            t->fichas[j][f].posicion   = 0;
        }
        t->stats[j].fichas_en_meta  = 0;
        t->stats[j].fichas_comidas  = 0;
        t->stats[j].fichas_perdidas = 0;
        t->stats[j].turnos_jugados  = 0;
    }
}

void tablero_init_sync(Tablero *t)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

    for (int i = 0; i < NUM_CASILLAS; i++)
        pthread_mutex_init(&t->mutex_casillas[i], &attr);

    for (int j = 0; j < NUM_JUGADORES; j++)
        for (int i = 0; i < NUM_CASILLAS_PASILLO; i++)
            pthread_mutex_init(&t->mutex_pasillos[j][i], &attr);

    pthread_mutex_init(&t->mutex_dado,  &attr);
    pthread_mutex_init(&t->mutex_turno, &attr);
    pthread_mutexattr_destroy(&attr);

    for (int j = 0; j < NUM_JUGADORES; j++) {

        sem_init(&t->sem_meta[j], 1, NUM_FICHAS);

        sem_init(&t->sem_pasillo[j], 1, 1);
    }
}


int tablero_casilla_libre(Tablero *t, int posicion)
{
    return t->casillas[posicion].num_fichas == 0;
}

int tablero_casilla_segura(int posicion)
{
    for (int i = 0; i < NUM_CASILLAS_SEGURAS; i++)
        if (CASILLAS_SEGURAS[i] == posicion)
            return 1;
    return 0;
}

int tablero_pos_salida(int jugador)
{
    const int salidas[NUM_JUGADORES] = {
        SALIDA_ROJO, SALIDA_VERDE, SALIDA_AZUL, SALIDA_AMARILLO
    };
    return salidas[jugador];
}

int tablero_pos_pasillo_entrada(int jugador)
{
    const int entradas[NUM_JUGADORES] = {
        SALIDA_ROJO - 1, SALIDA_VERDE - 1, SALIDA_AZUL - 1, SALIDA_AMARILLO - 1
    };
    return entradas[jugador];
}

/* Saca una ficha de su casilla del tablero (sección crítica de esa casilla) */
static void liberar_casilla(Tablero *t, int posicion)
{
    pthread_mutex_lock(&t->mutex_casillas[posicion]);
    t->casillas[posicion].num_fichas--;
    if (t->casillas[posicion].num_fichas <= 0) {
        t->casillas[posicion].num_fichas      = 0;
        t->casillas[posicion].ocupante_jugador = -1;
        t->casillas[posicion].ocupante_ficha   = -1;
    }
    pthread_mutex_unlock(&t->mutex_casillas[posicion]);
}

int tablero_mover_ficha(Tablero *t, int jugador, int ficha, int pasos)
{
    Ficha *f = &t->fichas[jugador][ficha];

    if (f->estado == EN_META)
        return 0;

    /* ── Salir de la base (solo con 5) ── */
    if (f->estado == EN_BASE) {
        if (pasos != 5)
            return 0;
        int salida = tablero_pos_salida(jugador);
        pthread_mutex_lock(&t->mutex_casillas[salida]);
        if (!tablero_casilla_libre(t, salida) &&
            !tablero_casilla_segura(salida) &&
            t->casillas[salida].ocupante_jugador != jugador) {
            tablero_comer_ficha(t, jugador, salida);
        }
        f->estado   = EN_TABLERO;
        f->posicion = salida;
        t->casillas[salida].num_fichas++;
        t->casillas[salida].ocupante_jugador = jugador;
        t->casillas[salida].ocupante_ficha   = ficha;
        pthread_mutex_unlock(&t->mutex_casillas[salida]);
        return 1;
    }

    /* ── Movimiento en el anillo ── */
    if (f->estado == EN_TABLERO) {
        const int entradas_pasillo[NUM_JUGADORES] = {
            SALIDA_ROJO - 1, SALIDA_VERDE - 1, SALIDA_AZUL - 1, SALIDA_AMARILLO - 1
        };
        int entrada = entradas_pasillo[jugador];
        int pasos_hasta_entrada = (entrada - f->posicion + NUM_CASILLAS) % NUM_CASILLAS;

        if (pasos_hasta_entrada < pasos) {
            int pasos_en_pasillo = pasos - pasos_hasta_entrada;


            if (pasos_en_pasillo >= NUM_CASILLAS_PASILLO) {
                sem_wait(&t->sem_meta[jugador]); 
                liberar_casilla(t, f->posicion);
                f->estado   = EN_META;
                f->posicion = 0;
                t->stats[jugador].fichas_en_meta++;
                return 1;
            }


            if (sem_trywait(&t->sem_pasillo[jugador]) != 0)
                return 0;

            liberar_casilla(t, f->posicion);
            f->estado   = EN_PASILLO;
            f->posicion = pasos_en_pasillo;
            pthread_mutex_lock(&t->mutex_pasillos[jugador][f->posicion]);
            t->pasillos[jugador][f->posicion].ocupante_jugador = jugador;
            t->pasillos[jugador][f->posicion].ocupante_ficha   = ficha;
            pthread_mutex_unlock(&t->mutex_pasillos[jugador][f->posicion]);
            return 1;
        }

        int nueva_pos = (f->posicion + pasos) % NUM_CASILLAS;
        liberar_casilla(t, f->posicion);

        pthread_mutex_lock(&t->mutex_casillas[nueva_pos]);
        if (!tablero_casilla_libre(t, nueva_pos) &&
            !tablero_casilla_segura(nueva_pos) &&
            t->casillas[nueva_pos].ocupante_jugador != jugador) {
            tablero_comer_ficha(t, jugador, nueva_pos);
        }
        f->posicion = nueva_pos;
        t->casillas[nueva_pos].num_fichas++;
        t->casillas[nueva_pos].ocupante_jugador = jugador;
        t->casillas[nueva_pos].ocupante_ficha   = ficha;
        pthread_mutex_unlock(&t->mutex_casillas[nueva_pos]);
        return 1;
    }

    /* ── Movimiento dentro del pasillo (la ficha YA retiene sem_pasillo) ── */
    if (f->estado == EN_PASILLO) {
        int nueva_pos = f->posicion + pasos;

        /* Llega a la meta: libera el pasillo y reserva plaza en meta */
        if (nueva_pos >= NUM_CASILLAS_PASILLO) {
            pthread_mutex_lock(&t->mutex_pasillos[jugador][f->posicion]);
            t->pasillos[jugador][f->posicion].ocupante_jugador = -1;
            t->pasillos[jugador][f->posicion].ocupante_ficha   = -1;
            pthread_mutex_unlock(&t->mutex_pasillos[jugador][f->posicion]);

            sem_post(&t->sem_pasillo[jugador]);   /* libera el pasillo estrecho */
            sem_wait(&t->sem_meta[jugador]);      /* reserva plaza en meta      */
            f->estado   = EN_META;
            f->posicion = 0;
            t->stats[jugador].fichas_en_meta++;
            return 1;
        }

        /* Avance dentro del pasillo: no toca el semáforo (lo sigue reteniendo) */
        pthread_mutex_lock(&t->mutex_pasillos[jugador][f->posicion]);
        t->pasillos[jugador][f->posicion].ocupante_jugador = -1;
        t->pasillos[jugador][f->posicion].ocupante_ficha   = -1;
        pthread_mutex_unlock(&t->mutex_pasillos[jugador][f->posicion]);

        pthread_mutex_lock(&t->mutex_pasillos[jugador][nueva_pos]);
        f->posicion = nueva_pos;
        t->pasillos[jugador][nueva_pos].ocupante_jugador = jugador;
        t->pasillos[jugador][nueva_pos].ocupante_ficha   = ficha;
        pthread_mutex_unlock(&t->mutex_pasillos[jugador][nueva_pos]);
        return 1;
    }

    return 0;
}

void tablero_comer_ficha(Tablero *t, int jugador_atacante, int posicion)
{
    int j_rival = t->casillas[posicion].ocupante_jugador;
    if (j_rival < 0 || j_rival == jugador_atacante) return;

    for (int f = 0; f < NUM_FICHAS; f++) {
        if (t->fichas[j_rival][f].estado   == EN_TABLERO &&
            t->fichas[j_rival][f].posicion == posicion) {
            t->fichas[j_rival][f].estado   = EN_BASE;
            t->fichas[j_rival][f].posicion = 0;
            t->stats[jugador_atacante].fichas_comidas++;
            t->stats[j_rival].fichas_perdidas++;
        }
    }

    t->casillas[posicion].ocupante_jugador = -1;
    t->casillas[posicion].ocupante_ficha   = -1;
    t->casillas[posicion].num_fichas       = 0;
}

/* Faltaba su definición: estaba declarada en tablero.h pero no implementada */
int tablero_ficha_llego_meta(Tablero *t, int jugador, int ficha)
{
    return t->fichas[jugador][ficha].estado == EN_META;
}

int tablero_jugador_gano(Tablero *t, int jugador)
{
    return t->stats[jugador].fichas_en_meta == NUM_FICHAS;
}