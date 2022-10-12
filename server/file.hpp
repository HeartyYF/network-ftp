#ifndef __FTP_FILE__
#define __FTP_FILE__

#include "config.hpp"
#include "utils.hpp"
#include "logs.hpp"

#include <dirent.h>
#include <sys/stat.h>

int check_dir(char* path) {
    DIR* d = opendir(path);
    if (d) {
        closedir(d);
        return 1;
    }
    return 0;
}

int check_file(char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return 1;
    }
    return 0;
}

int check_file_read(char* path) {
    struct stat st;
    if (stat(path, &st) == 0 && st.st_mode & S_IRUSR) {
        return 1;
    }
    return 0;
}

int file_init() {
    if (check_dir(ROOT) == 0) {
        return -1;
    }
    return 1;
}

char* list_dir(char* path, int more, char* filebuf) {
    DIR* d = opendir(path);
    if (d == NULL) {
        return NULL;
    }
    struct dirent* dir;
    int i = 0;
    while ((dir = readdir(d)) != NULL) {
        char* lbuf = (char*)malloc(size);
        if (more) {
            struct stat st;
            char* fullpath = path_concat(path, dir->d_name);
            stat(fullpath, &st);
            free(fullpath);
            sprintf(lbuf, "%s %13lld %c %s\r\n", remove_crlf(ctime(&st.st_mtime)), (long long)st.st_size,
                    S_ISDIR(st.st_mode) ? 'd' : '-', dir->d_name);
        } else {
            sprintf(lbuf, "%s\r\n", dir->d_name);
        }
        if (i + strlen(lbuf) + 1 >= size) {
            free(lbuf);
            return NULL;
        }
        strcpy(filebuf + i, lbuf);
        i += strlen(lbuf);
        free(lbuf);
    }
    filebuf[i] = '\0';
    closedir(d);
    return filebuf;
}

#endif