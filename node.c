#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define NAME_SIZE 64
#define ADDR_SIZE 64

typedef struct {
    char name[NAME_SIZE];
    char addr[ADDR_SIZE];
    int port;
    int server_fd;
} NodeConfig;

void die(const char *msg) {
    perror(msg);
    exit(1);
}

void *receiver_thread(void *arg) {
    NodeConfig *cfg = (NodeConfig *)arg;

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int conn_fd = accept(cfg->server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept");
            continue;
        }

        char buf[BUF_SIZE];
        memset(buf, 0, sizeof(buf));

        ssize_t n = recv(conn_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            close(conn_fd);
            continue;
        }

        printf("[%s] received: %s\n", cfg->name, buf);

        if (strncmp(buf, "PING", 4) == 0) {
            char sender_name[NAME_SIZE];
            char sender_addr[ADDR_SIZE];

            memset(sender_name, 0, sizeof(sender_name));
            memset(sender_addr, 0, sizeof(sender_addr));

            // Expected format: PING <sender_name> <sender_addr>
            if (sscanf(buf, "PING %63s %63s", sender_name, sender_addr) == 2) {
                char reply[BUF_SIZE];
                snprintf(reply, sizeof(reply),
                         "PONG from %s (%s) to %s (%s)",
                         cfg->name, cfg->addr, sender_name, sender_addr);
                send(conn_fd, reply, strlen(reply), 0);
            }
        }

        close(conn_fd);
    }

    return NULL;
}

int create_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        die("socket");

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        die("setsockopt");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        die("bind");

    if (listen(server_fd, 5) < 0)
        die("listen");

    return server_fd;
}

void do_ping(NodeConfig *cfg, const char *target_name, const char *target_addr, int target_port) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr("127.0.0.1");
    target.sin_port = htons((uint16_t)target_port);

    if (connect(sock_fd, (struct sockaddr *)&target, sizeof(target)) < 0) {
        perror("connect");
        close(sock_fd);
        return;
    }

    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "PING %s %s", cfg->name, cfg->addr);

    if (send(sock_fd, msg, strlen(msg), 0) < 0) {
        perror("send");
        close(sock_fd);
        return;
    }

    char reply[BUF_SIZE];
    memset(reply, 0, sizeof(reply));

    ssize_t n = recv(sock_fd, reply, sizeof(reply) - 1, 0);
    if (n < 0) {
        perror("recv");
        close(sock_fd);
        return;
    }

    printf("[%s] reply from %s (%s:%d): %s\n",
           cfg->name, target_name, target_addr, target_port, reply);

    close(sock_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <node_name> <logical_ip> <port>\n", argv[0]);
        fprintf(stderr, "Example: %s ns1 192.168.1.1 5001\n", argv[0]);
        return 1;
    }

    NodeConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    strncpy(cfg.name, argv[1], sizeof(cfg.name) - 1);
    strncpy(cfg.addr, argv[2], sizeof(cfg.addr) - 1);
    cfg.port = atoi(argv[3]);

    if (cfg.port <= 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[3]);
        return 1;
    }

    cfg.server_fd = create_server(cfg.port);

    printf("%s started with logical IP %s, listening on localhost:%d\n",
           cfg.name, cfg.addr, cfg.port);

    pthread_t tid;
    if (pthread_create(&tid, NULL, receiver_thread, &cfg) != 0)
        die("pthread_create");

    char line[BUF_SIZE];

    while (1) {
        printf("%s> ", cfg.name);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "exit") == 0) {
            break;
        } else if (strncmp(line, "ping ", 5) == 0) {
            char target_name[NAME_SIZE];
            char target_addr[ADDR_SIZE];
            int target_port;

            memset(target_name, 0, sizeof(target_name));
            memset(target_addr, 0, sizeof(target_addr));
            target_port = 0;

            // Usage: ping <target_name> <target_addr> <target_port>
            if (sscanf(line, "ping %63s %63s %d",
                       target_name, target_addr, &target_port) == 3) {
                do_ping(&cfg, target_name, target_addr, target_port);
            } else {
                printf("Usage: ping <target_name> <target_addr> <target_port>\n");
            }
        } else if (strlen(line) == 0) {
            continue;
        } else {
            printf("Commands:\n");
            printf("  ping <target_name> <target_addr> <target_port>\n");
            printf("  exit\n");
        }
    }

    close(cfg.server_fd);
    return 0;
}
