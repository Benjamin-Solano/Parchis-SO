#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>

#include "../include/tipos.h"
#include "../include/tablero.h"
#include "../include/arbitro.h"
#include "../include/jugador.h"
#include "../include/ipc.h"
#include "../include/visualizacion.h"

int main(void)
{
    /* ── 1. Crear tablero en memoria compartida ── */
    int shmid = 0;
    Tablero *tablero = tablero_crear();
    if (!tablero) {
        perror("tablero_crear");
        exit(EXIT_FAILURE);
    }
    tablero_init(tablero);

    /* ── 2. Crear cola de mensajes para eventos ── */
    int msqid = msgq_crear();
    if (msqid < 0) {
        perror("msgq_crear");
        exit(EXIT_FAILURE);
    }

    /* ── 3. Crear pipes (uno por jugador) para estadísticas ── */
    int pipes[NUM_JUGADORES][2];
    for (int i = 0; i < NUM_JUGADORES; i++) {
        if (pipe_crear(pipes[i]) < 0) {
            perror("pipe_crear");
            exit(EXIT_FAILURE);
        }
    }

    /* ── 4. Crear sockets para comunicación árbitro <-> jugadores ── */
    int socket_pares[NUM_JUGADORES][2];
    for (int i = 0; i < NUM_JUGADORES; i++) {
        if (socket_crear_par(socket_pares[i]) < 0) {
            perror("socket_crear_par");
            exit(EXIT_FAILURE);
        }
    }

    /* ── 5. fork() — un proceso hijo por jugador ── */
    pid_t pids[NUM_JUGADORES];
    for (int i = 0; i < NUM_JUGADORES; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) {
            /* Proceso hijo: cerrar extremos que no usa */
            close(pipes[i][0]);
            for (int j = 0; j < NUM_JUGADORES; j++) {
                if (j != i) {
                    close(socket_pares[j][1]);
                    close(socket_pares[j][0]);
                }
            }
            close(socket_pares[i][0]); /* hijo usa el extremo [1] */

            /* Registrar PID en tablero compartido */
            tablero->pids[i] = getpid();

            jugador_proceso(i, tablero, socket_pares[i][1]);

            /* Enviar estadísticas al padre via pipe */
            pipe_enviar_stats(pipes[i][1], i, &tablero->stats[i]);
            close(pipes[i][1]);
            exit(EXIT_SUCCESS);
        }

        /* Padre: cerrar extremos del hijo */
        close(socket_pares[i][1]);
        close(pipes[i][1]);
    }

    /* ── 6. Proceso padre: actúa como Árbitro ── */
    Arbitro arbitro;
    arbitro_init(&arbitro);
    for (int i = 0; i < NUM_JUGADORES; i++)
        arbitro.socket_fds[i] = socket_pares[i][0];

    vis_dibujar_tablero(tablero);
    arbitro_loop(&arbitro, tablero, pids);

    /* ── 7. Recolectar estadísticas de los hijos ── */
    EstadisticasJugador stats_finales[NUM_JUGADORES];
    int jugador_id;
    for (int i = 0; i < NUM_JUGADORES; i++) {
        pipe_recibir_stats(pipes[i][0], &jugador_id, &stats_finales[jugador_id]);
        close(pipes[i][0]);
    }

    /* ── 8. wait() — esperar a todos los hijos ── */
    for (int i = 0; i < NUM_JUGADORES; i++)
        waitpid(pids[i], NULL, 0);

    /* ── 9. Mostrar resultados y liberar recursos ── */
    vis_mostrar_stats(tablero);
    vis_mostrar_ganador(tablero->ganador);

    msgq_destruir(msqid);
    tablero_destruir(tablero, shmid);

    return EXIT_SUCCESS;
}
