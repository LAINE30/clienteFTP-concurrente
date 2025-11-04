#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include "connectsock.h"
#include "errexit.h"

#define BUFSIZE 8192

/* send a line to socket */
ssize_t sendline(int fd, const char *s) {
    size_t len = strlen(s);
    return send(fd, s, len, 0);
}

/* receive a single reply line (blocking) */
int recv_reply(int control_fd, char *buf, size_t buflen) {
    ssize_t n = recv(control_fd, buf, buflen-1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return 0;
}

/* read full reply possibly multi-line; returns numeric code in buf_code */
int read_response(int ctrl, char *out, int outlen) {
    char buf[1024];
    if (recv_reply(ctrl, buf, sizeof buf) < 0) return -1;
    /* Many servers send complete reply in one recv, this is a simple approach */
    strncpy(out, buf, outlen-1);
    out[outlen-1] = '\0';
    return 0;
}

/* parse PASV reply like: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2). */
int parse_pasv_response(const char *resp, char *ip, int *port) {
    const char *p = strchr(resp, '(');
    if (!p) return -1;
    int h1,h2,h3,h4,p1,p2;
    if (sscanf(p+1, "%d,%d,%d,%d,%d,%d", &h1,&h2,&h3,&h4,&p1,&p2) < 6) return -1;
    sprintf(ip, "%d.%d.%d.%d", h1,h2,h3,h4);
    *port = p1*256 + p2;
    return 0;
}

/* open data connection in PASV mode */
int open_pasv_data_conn(int ctrl_fd, char *server_host) {
    char cmd[] = "PASV\r\n";
    char resp[1024];
    sendline(ctrl_fd, cmd);
    if (read_response(ctrl_fd, resp, sizeof resp) < 0) return -1;
    /* expecting 227 */
    if (strncmp(resp, "227", 3) != 0) {
        fprintf(stderr, "PASV failed: %s\n", resp);
        return -1;
    }
    char ip[64];
    int port;
    if (parse_pasv_response(resp, ip, &port) < 0) {
        fprintf(stderr, "Could not parse PASV response: %s\n", resp);
        return -1;
    }
    /* connect to ip:port */
    char portstr[16];
    snprintf(portstr, sizeof portstr, "%d", port);
    int datafd = connectTCP(ip, portstr);
    if (datafd < 0) {
        perror("connect data");
        return -1;
    }
    return datafd;
}

/* open data connection in active (PORT) mode:
   create a listening socket, send PORT h1,h2,h3,h4,p1,p2, then accept */
int open_port_active(int ctrl_fd, int *out_listen_fd) {
    int listenfd;
    struct sockaddr_in serv;
    socklen_t len = sizeof(serv);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return -1; }

    memset(&serv, 0, sizeof serv);
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = 0; // ephemeral

    if (bind(listenfd, (struct sockaddr*)&serv, sizeof serv) < 0) {
        perror("bind"); close(listenfd); return -1;
    }

    if (listen(listenfd, 1) < 0) { perror("listen"); close(listenfd); return -1; }

    // get bound port
    if (getsockname(listenfd, (struct sockaddr*)&serv, &len) < 0) {
        perror("getsockname"); close(listenfd); return -1;
    }
    int port = ntohs(serv.sin_port);

    // determine local IP to send in PORT command
    char hostbuf[256];
    if (gethostname(hostbuf, sizeof hostbuf) < 0) {
        perror("gethostname");
        close(listenfd); return -1;
    }
    struct hostent *hent = gethostbyname(hostbuf);
    if (!hent) {
        perror("gethostbyname");
        close(listenfd); return -1;
    }
    unsigned char *ip = (unsigned char*)hent->h_addr_list[0];

    int p1 = port / 256;
    int p2 = port % 256;
    char portcmd[128];
    snprintf(portcmd, sizeof portcmd, "PORT %u,%u,%u,%u,%d,%d\r\n",
             ip[0], ip[1], ip[2], ip[3], p1, p2);
    sendline(ctrl_fd, portcmd);
    char resp[1024];
    if (read_response(ctrl_fd, resp, sizeof resp) < 0) {
        close(listenfd); return -1;
    }
    if (strncmp(resp, "200", 3) != 0) {
        fprintf(stderr, "PORT failed: %s\n", resp);
        close(listenfd); return -1;
    }
    *out_listen_fd = listenfd;
    return 0;
}

/* perform RETR in child: download remote->local */
void do_retr(int ctrl_fd, const char *remote, const char *local, int use_pasv) {
    int datafd = -1;
    int listenfd = -1;

    if (use_pasv) {
        datafd = open_pasv_data_conn(ctrl_fd, NULL);
        if (datafd < 0) {
            fprintf(stderr, "PASV data connection failed\n");
            exit(1);
        }
    } else {
        if (open_port_active(ctrl_fd, &listenfd) < 0) {
            fprintf(stderr, "PORT setup failed\n");
            exit(1);
        }
    }

    char cmd[512];
    snprintf(cmd, sizeof cmd, "RETR %s\r\n", remote);
    sendline(ctrl_fd, cmd);

    char resp[1024];
    if (read_response(ctrl_fd, resp, sizeof resp) < 0) {
        fprintf(stderr, "No response to RETR\n");
        exit(1);
    }
    if (strncmp(resp, "150", 3) != 0 && strncmp(resp, "125", 3) != 0) {
        fprintf(stderr, "Server refused RETR: %s\n", resp);
        if (listenfd >= 0) close(listenfd);
        exit(1);
    }

    if (!use_pasv) {
        struct sockaddr_in cli;
        socklen_t len = sizeof cli;
        datafd = accept(listenfd, (struct sockaddr*)&cli, &len);
        close(listenfd);
        if (datafd < 0) { perror("accept"); exit(1); }
    }

    int fd = open(local, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) { perror("open local file"); close(datafd); exit(1); }

    ssize_t n;
    char buf[BUFSIZE];
    while ((n = recv(datafd, buf, sizeof buf, 0)) > 0) {
        if (write(fd, buf, n) != n) {
            perror("write");
            break;
        }
    }
    close(fd); close(datafd);

    /* read final response */
    if (read_response(ctrl_fd, resp, sizeof resp) == 0) {
        // print final status
        fprintf(stderr, "RETR finished: %s", resp);
    }
    exit(0);
}

/* perform STOR in child: upload local->remote */
void do_stor(int ctrl_fd, const char *local, const char *remote, int use_pasv) {
    int datafd = -1;
    int listenfd = -1;

    if (use_pasv) {
        datafd = open_pasv_data_conn(ctrl_fd, NULL);
        if (datafd < 0) {
            fprintf(stderr, "PASV data connection failed\n");
            exit(1);
        }
    } else {
        if (open_port_active(ctrl_fd, &listenfd) < 0) {
            fprintf(stderr, "PORT setup failed\n");
            exit(1);
        }
    }

    char cmd[512];
    snprintf(cmd, sizeof cmd, "STOR %s\r\n", remote);
    sendline(ctrl_fd, cmd);

    char resp[1024];
    if (read_response(ctrl_fd, resp, sizeof resp) < 0) {
        fprintf(stderr, "No response to STOR\n");
        exit(1);
    }
    if (strncmp(resp, "150", 3) != 0 && strncmp(resp, "125", 3) != 0) {
        fprintf(stderr, "Server refused STOR: %s\n", resp);
        if (listenfd >= 0) close(listenfd);
        exit(1);
    }

    if (!use_pasv) {
        struct sockaddr_in cli;
        socklen_t len = sizeof cli;
        datafd = accept(listenfd, (struct sockaddr*)&cli, &len);
        close(listenfd);
        if (datafd < 0) { perror("accept"); exit(1); }
    }

    int fd = open(local, O_RDONLY);
    if (fd < 0) { perror("open local file"); close(datafd); exit(1); }

    ssize_t n;
    char buf[BUFSIZE];
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        if (send(datafd, buf, n, 0) != n) {
            perror("send");
            break;
        }
    }
    close(fd); close(datafd);

    /* read final response */
    if (read_response(ctrl_fd, resp, sizeof resp) == 0) {
        fprintf(stderr, "STOR finished: %s", resp);
    }
    exit(0);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <server> <port>\n", argv[0]);
        exit(1);
    }
    const char *server = argv[1];
    const char *port = argv[2];

    int ctrl = connectTCP(server, port);
    if (ctrl < 0) errexit("No se pudo conectar al servidor %s:%s", server, port);

    char buf[1024];
    if (read_response(ctrl, buf, sizeof buf) == 0)
        printf("%s", buf); /* welcome */

    int use_pasv = 1; /* default PASV */

    char line[512];
    while (1) {
        printf("ftp> ");
        if (!fgets(line, sizeof line, stdin)) break;

        /* trim newline */
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "quit", 4) == 0 || strncmp(line, "exit", 4) == 0) {
            sendline(ctrl, "QUIT\r\n");
            if (read_response(ctrl, buf, sizeof buf) == 0) printf("%s", buf);
            break;
        } else if (strncmp(line, "user ", 5) == 0) {
            char cmd[512];
            snprintf(cmd, sizeof cmd, "USER %s\r\n", line+5);
            sendline(ctrl, cmd);
            if (read_response(ctrl, buf, sizeof buf) == 0) printf("%s", buf);
        } else if (strncmp(line, "pass ", 5) == 0) {
            char cmd[512];
            snprintf(cmd, sizeof cmd, "PASS %s\r\n", line+5);
            sendline(ctrl, cmd);
            if (read_response(ctrl, buf, sizeof buf) == 0) printf("%s", buf);
        } else if (strcmp(line, "pasv") == 0) {
            use_pasv = 1;
            printf("Modo PASV seleccionado\n");
        } else if (strcmp(line, "port") == 0) {
            use_pasv = 0;
            printf("Modo PORT (activo) seleccionado\n");
        } else if (strncmp(line, "retr ", 5) == 0) {
            /* retr remote [local] */
            char *args = line + 5;
            char *remote = strtok(args, " ");
            char *local = strtok(NULL, " ");
            if (!remote) { fprintf(stderr, "Uso: retr remote [local]\n"); continue; }
            if (!local) local = remote;

            pid_t pid = fork();
            if (pid < 0) { perror("fork"); continue; }
            if (pid == 0) {
                /* child performs transfer then exits */
                do_retr(ctrl, remote, local, use_pasv);
                /* never returns */
            } else {
                /* parent: continue (reap finished children) */
                int status;
                while (waitpid(-1, &status, WNOHANG) > 0) {}
                printf("Transferencia RETR iniciada en proceso %d\n", pid);
            }
        } else if (strncmp(line, "stor ", 5) == 0) {
            /* stor local [remote] */
            char *args = line + 5;
            char *local = strtok(args, " ");
            char *remote = strtok(NULL, " ");
            if (!local) { fprintf(stderr, "Uso: stor local [remote]\n"); continue; }
            if (!remote) remote = local;

            pid_t pid = fork();
            if (pid < 0) { perror("fork"); continue; }
            if (pid == 0) {
                do_stor(ctrl, local, remote, use_pasv);
            } else {
                int status;
                while (waitpid(-1, &status, WNOHANG) > 0) {}
                printf("Transferencia STOR iniciada en proceso %d\n", pid);
            }
        } else {
            /* send arbitrary command through control connection */
            char cmd[600];
            snprintf(cmd, sizeof cmd, "%s\r\n", line);
            sendline(ctrl, cmd);
            if (read_response(ctrl, buf, sizeof buf) == 0) {
                printf("%s", buf);
            } else {
                printf("No response or connection closed\n");
            }
        }
    }

    close(ctrl);
    return 0;
}
