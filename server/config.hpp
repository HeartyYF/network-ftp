#ifndef __FTP_CONFIG__
#define __FTP_CONFIG__

#include <string.h>
#include <stdlib.h>

#define size 8192

int PORT = 21;
char* ROOT = NULL;
int MAX_CLIENTS = 10;
int current_clients = 0;

void parse_config(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
            PORT = atoi(argv[i + 1]);
        } else if ((strcmp(argv[i], "-root") == 0 || strcmp(argv[i], "-r") == 0) && i + 1 < argc) {
            ROOT = argv[i + 1];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            MAX_CLIENTS = atoi(argv[i + 1]);
        }
    }
    if (ROOT == NULL) {
        ROOT = (char*)malloc(5);
        strcpy(ROOT, "/tmp");
    }
}

#endif