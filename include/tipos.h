#ifndef TIPOS_H
#define TIPOS_H

#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>

/* ── Constantes del juego ── */
#define NUM_JUGADORES       4
#define NUM_FICHAS          4
#define NUM_CASILLAS        68
#define NUM_CASILLAS_PASILLO 7
#define NUM_CASILLAS_META    1

/* Casillas seguras (índices globales en el tablero) */
#define NUM_CASILLAS_SEGURAS 16
extern const int CASILLAS_SEGURAS[NUM_CASILLAS_SEGURAS];

/* Casillas de salida por jugador (índice en el tablero global) */
#define SALIDA_ROJO    5
#define SALIDA_VERDE   22
#define SALIDA_AZUL    39
#define SALIDA_AMARILLO 56

/* Inicio del pasillo final por jugador (índice donde empieza su pasillo) */
#define PASILLO_ROJO    68
#define PASILLO_VERDE   82
#define PASILLO_AZUL    96
#define PASILLO_AMARILLO 110

/* ── Identificadores de jugadores ── */
typedef enum {
    ROJO    = 0,
    VERDE   = 1,
    AZUL    = 2,
    AMARILLO = 3
} ColorJugador;

/* ── Estado de una ficha ── */
typedef enum {
    EN_BASE    = 0,
    EN_TABLERO = 1,
    EN_PASILLO = 2,
    EN_META    = 3
} EstadoFicha;

/* ── Representación de una ficha ── */
typedef struct {
    int          jugador_id;
    int          ficha_id;
    EstadoFicha  estado;
    int          posicion;   /* 0-67 tablero global, 0-6 pasillo */
} Ficha;

/* ── Una casilla del tablero ── */
typedef struct {
    int  ocupante_jugador;   /* -1 si libre */
    int  ocupante_ficha;     /* -1 si libre */
    int  es_segura;
} Casilla;

/* ── Estadísticas por jugador ── */
typedef struct {
    int fichas_en_meta;
    int fichas_comidas;
    int fichas_perdidas;
    int turnos_jugados;
} EstadisticasJugador;

/* ── Estado global del tablero (en memoria compartida) ── */
typedef struct {
    Casilla              casillas[NUM_CASILLAS];
    Casilla              pasillos[NUM_JUGADORES][NUM_CASILLAS_PASILLO];
    Ficha                fichas[NUM_JUGADORES][NUM_FICHAS];
    EstadisticasJugador  stats[NUM_JUGADORES];

    int  turno_actual;
    int  dado;               /* resultado del último lanzamiento */
    int  partida_terminada;
    int  ganador;            /* -1 si no hay ganador aún */

    /* Sincronización */
    pthread_mutex_t mutex_casillas[NUM_CASILLAS];
    pthread_mutex_t mutex_pasillos[NUM_JUGADORES][NUM_CASILLAS_PASILLO];
    pthread_mutex_t mutex_dado;
    pthread_mutex_t mutex_turno;
    sem_t           sem_meta[NUM_JUGADORES];
    sem_t           sem_pasillo[NUM_JUGADORES];

    /* PIDs de los procesos jugadores */
    pid_t pids[NUM_JUGADORES];
} Tablero;

/* ── Argumentos que recibe cada hilo-ficha ── */
typedef struct {
    int      jugador_id;
    int      ficha_id;
    Tablero *tablero;
} ArgsHilo;

/* ── Mensaje IPC entre procesos ── */
#define MSG_TIPO_TURNO      1
#define MSG_TIPO_EVENTO     2
#define MSG_TIPO_STATS      3
#define MSG_TIPO_FIN        4

typedef struct {
    long tipo;
    int  jugador_origen;
    int  jugador_destino;
    int  dato;           /* dado, ficha_id, posicion, etc. */
    char texto[64];
} MensajeIPC;

#endif /* TIPOS_H */
