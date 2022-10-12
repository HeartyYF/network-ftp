#include "logs.hpp"
#include "utils.hpp"
#include "types.hpp"
#include "config.hpp"
#include "network.hpp"
#include "file.hpp"

extern int PORT;
extern char* ROOT;
extern int MAX_CLIENTS;
extern int current_clients;

int main(int argc, char** argv) {
    parse_config(argc, argv);

    logs(LOG_TYPE_INFO, "Starting server");
    logs(LOG_TYPE_INFO, "Port: %d", PORT);
    logs(LOG_TYPE_INFO, "Root: %s", ROOT);
    logs(LOG_TYPE_INFO, "Max clients: %d", MAX_CLIENTS);

    if (file_init() < 0) {
        logs(LOG_TYPE_ERRO, "Failed to init root directory");
        return -1;
    }

    int listenfd = open_main(PORT);
    if (listenfd < 0) {
        logs(LOG_TYPE_ERRO, "Failed to open main socket");
        return 1;
    }

    logs(LOG_TYPE_INFO, "Server started");

    while (1) {
        struct sockaddr_in* client_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        memset(client_addr, 0, sizeof(struct sockaddr_in));
        socklen_t client_addr_len = sizeof(struct sockaddr_in);
        int connfd = accept(listenfd, (struct sockaddr*) client_addr, &client_addr_len);
        if (connfd < 0) {
            logs(LOG_TYPE_ERRO, "Failed to accept connection. Error: %s, %d", strerror(errno), errno);
            free(client_addr);
            continue;
        }
        logs(LOG_TYPE_INFO, "Accepted connection from %s:%d", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
        struct ftp_connection* f_conn = (struct ftp_connection*)malloc(sizeof(struct ftp_connection));
        memset(f_conn, 0, sizeof(struct ftp_connection));
        f_conn->connfd = connfd;
        f_conn->client_addr = client_addr;
        int rc = pthread_create(&f_conn->tid, NULL, (void*(*)(void*))ftp_connection_thread, (void*)f_conn);
        if (rc < 0) {
            logs(LOG_TYPE_ERRO, "Failed to accept connection. Error: %s, %d", strerror(errno), errno);
            free(client_addr);
            free(f_conn);
            continue;
        }
    }

    logs(LOG_TYPE_INFO, "Server stopped");
    shutdown(listenfd, SHUT_RDWR);
    close(listenfd);
    return 0;
}