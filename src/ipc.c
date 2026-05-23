#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "../include/ipc.h"

/* ────────────────────────────────────────────
   Sockets (socketpair — AF_UNIX)
   ──────────────────────────────────────────── */

int socket_crear_par(int fds[2])
{
    return socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
}

void socket_enviar(int fd, const MensajeIPC *msg)
{
    ssize_t n = write(fd, msg, sizeof(MensajeIPC));
    if (n != sizeof(MensajeIPC))
        perror("socket_enviar");
}

int socket_recibir(int fd, MensajeIPC *msg)
{
    ssize_t n = read(fd, msg, sizeof(MensajeIPC));
    if (n <= 0) {
        if (n < 0) perror("socket_recibir");
        return -1;
    }
    return 0;
}

void socket_cerrar(int fd)
{
    close(fd);
}

/* ────────────────────────────────────────────
   Colas de mensajes POSIX System V
   ──────────────────────────────────────────── */

#define MSG_KEY  0x50415243   /* "PARC" en hex */

int msgq_crear(void)
{
    int id = msgget((key_t)MSG_KEY, IPC_CREAT | 0666);
    if (id < 0) perror("msgget");
    return id;
}

void msgq_enviar(int msqid, const MensajeIPC *msg)
{
    if (msgsnd(msqid, msg, sizeof(MensajeIPC) - sizeof(long), 0) < 0)
        perror("msgsnd");
}

int msgq_recibir(int msqid, MensajeIPC *msg, long tipo)
{
    ssize_t n = msgrcv(msqid, msg,
                       sizeof(MensajeIPC) - sizeof(long),
                       tipo, 0);
    if (n < 0) {
        perror("msgrcv");
        return -1;
    }
    return 0;
}

void msgq_destruir(int msqid)
{
    if (msgctl(msqid, IPC_RMID, NULL) < 0)
        perror("msgctl IPC_RMID");
}

/* ────────────────────────────────────────────
   Pipes
   ──────────────────────────────────────────── */

typedef struct {
    int                jugador_id;
    EstadisticasJugador stats;
} PipeStats;

int pipe_crear(int fds[2])
{
    return pipe(fds);
}

void pipe_enviar_stats(int fd, int jugador_id, const EstadisticasJugador *stats)
{
    PipeStats ps = { jugador_id, *stats };
    ssize_t n = write(fd, &ps, sizeof(ps));
    if (n != sizeof(ps)) perror("pipe_enviar_stats");
}

int pipe_recibir_stats(int fd, int *jugador_id, EstadisticasJugador *stats)
{
    PipeStats ps;
    ssize_t n = read(fd, &ps, sizeof(ps));
    if (n != sizeof(ps)) {
        perror("pipe_recibir_stats");
        return -1;
    }
    *jugador_id = ps.jugador_id;
    *stats      = ps.stats;
    return 0;
}
