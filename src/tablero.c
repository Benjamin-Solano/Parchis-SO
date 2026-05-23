#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* ────────────────────────────────────────────
   Memoria compartida
   ──────────────────────────────────────────── */

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
    /* Reservado para uso con shmget si se prefiere */
    (void)shmid;
    return NULL;
}

void tablero_destruir(Tablero *t, int shmid)
{
    (void)shmid;
    if (!t) return;

    /* Destruir mutex de casillas */
    for (int i = 0; i < NUM_CASILLAS; i++)
        pthread_mutex_destroy(&t->mutex_casillas[i]);

    /* Destruir mutex de pasillos */
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

/* ────────────────────────────────────────────
   Inicialización
   ──────────────────────────────────────────── */

void tablero_init(Tablero *t)
{
    memset(t, 0, sizeof(Tablero));

    /* Casillas vacías */
    for (int i = 0; i < NUM_CASILLAS; i++) {
        t->casillas[i].ocupante_jugador = -1;
        t->casillas[i].ocupante_ficha   = -1;
        t->casillas[i].es_segura        = 0;
    }

    /* Marcar casillas seguras */
    for (int i = 0; i < NUM_CASILLAS_SEGURAS; i++) {
        int idx = CASILLAS_SEGURAS[i];
        if (idx >= 0 && idx < NUM_CASILLAS)
            t->casillas[idx].es_segura = 1;
    }

    /* Pasillos vacíos */
    for (int j = 0; j < NUM_JUGADORES; j++)
        for (int i = 0; i < NUM_CASILLAS_PASILLO; i++) {
            t->pasillos[j][i].ocupante_jugador = -1;
            t->pasillos[j][i].ocupante_ficha   = -1;
        }

    t->turno_actual    = ROJO;
    t->dado            = 0;
    t->partida_terminada = 0;
    t->ganador         = -1;

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

    sem_t *s;
    for (int j = 0; j < NUM_JUGADORES; j++) {
        /* Meta: máximo 4 fichas simultáneas (una por ficha del jugador) */
        sem_init(&t->sem_meta[j], 1, NUM_FICHAS);
        /* Pasillo: máximo 1 ficha a la vez en casillas estrechas */
        sem_init(&t->sem_pasillo[j], 1, 1);
        (void)s;
    }
}

/* ────────────────────────────────────────────
   Lógica del tablero
   ──────────────────────────────────────────── */

int tablero_casilla_libre(Tablero *t, int posicion)
{
    return t->casillas[posicion].ocupante_jugador == -1;
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
        PASILLO_ROJO - 68,
        PASILLO_VERDE - 68,
        PASILLO_AZUL - 68,
        PASILLO_AMARILLO - 68
    };
    return entradas[jugador];
}

int tablero_mover_ficha(Tablero *t, int jugador, int ficha, int pasos)
{
    Ficha *f = &t->fichas[jugador][ficha];

    if (f->estado == EN_META)
        return 0;

    if (f->estado == EN_BASE) {
        /* Solo sale con 5 */
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
        t->casillas[salida].ocupante_jugador = jugador;
        t->casillas[salida].ocupante_ficha   = ficha;
        pthread_mutex_unlock(&t->mutex_casillas[salida]);
        return 1;
    }

    if (f->estado == EN_TABLERO) {
        int nueva_pos = (f->posicion + pasos) % NUM_CASILLAS;

        /* TODO: detectar si entra al pasillo */

        pthread_mutex_lock(&t->mutex_casillas[f->posicion]);
        t->casillas[f->posicion].ocupante_jugador = -1;
        t->casillas[f->posicion].ocupante_ficha   = -1;
        pthread_mutex_unlock(&t->mutex_casillas[f->posicion]);

        pthread_mutex_lock(&t->mutex_casillas[nueva_pos]);
        if (!tablero_casilla_libre(t, nueva_pos) &&
            !tablero_casilla_segura(nueva_pos) &&
            t->casillas[nueva_pos].ocupante_jugador != jugador) {
            tablero_comer_ficha(t, jugador, nueva_pos);
        }
        f->posicion = nueva_pos;
        t->casillas[nueva_pos].ocupante_jugador = jugador;
        t->casillas[nueva_pos].ocupante_ficha   = ficha;
        pthread_mutex_unlock(&t->mutex_casillas[nueva_pos]);
        return 1;
    }

    if (f->estado == EN_PASILLO) {
        sem_wait(&t->sem_pasillo[jugador]);
        int nueva_pos = f->posicion + pasos;
        if (nueva_pos >= NUM_CASILLAS_PASILLO) {
            /* Llega a la meta */
            pthread_mutex_lock(&t->mutex_pasillos[jugador][f->posicion]);
            t->pasillos[jugador][f->posicion].ocupante_jugador = -1;
            t->pasillos[jugador][f->posicion].ocupante_ficha   = -1;
            pthread_mutex_unlock(&t->mutex_pasillos[jugador][f->posicion]);

            f->estado   = EN_META;
            f->posicion = 0;
            t->stats[jugador].fichas_en_meta++;
            sem_post(&t->sem_pasillo[jugador]);
            sem_wait(&t->sem_meta[jugador]);
            return 1;
        }
        pthread_mutex_lock(&t->mutex_pasillos[jugador][f->posicion]);
        t->pasillos[jugador][f->posicion].ocupante_jugador = -1;
        t->pasillos[jugador][f->posicion].ocupante_ficha   = -1;
        pthread_mutex_unlock(&t->mutex_pasillos[jugador][f->posicion]);

        pthread_mutex_lock(&t->mutex_pasillos[jugador][nueva_pos]);
        f->posicion = nueva_pos;
        t->pasillos[jugador][nueva_pos].ocupante_jugador = jugador;
        t->pasillos[jugador][nueva_pos].ocupante_ficha   = ficha;
        pthread_mutex_unlock(&t->mutex_pasillos[jugador][nueva_pos]);
        sem_post(&t->sem_pasillo[jugador]);
        return 1;
    }

    return 0;
}

void tablero_comer_ficha(Tablero *t, int jugador_atacante, int posicion)
{
    int j_rival = t->casillas[posicion].ocupante_jugador;
    int f_rival = t->casillas[posicion].ocupante_ficha;
    if (j_rival < 0) return;

    t->fichas[j_rival][f_rival].estado   = EN_BASE;
    t->fichas[j_rival][f_rival].posicion = 0;
    t->casillas[posicion].ocupante_jugador = -1;
    t->casillas[posicion].ocupante_ficha   = -1;

    t->stats[jugador_atacante].fichas_comidas++;
    t->stats[j_rival].fichas_perdidas++;
}

int tablero_jugador_gano(Tablero *t, int jugador)
{
    return t->stats[jugador].fichas_en_meta == NUM_FICHAS;
}
