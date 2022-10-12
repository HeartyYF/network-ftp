#ifndef __FTP_UTILS__
#define __FTP_UTILS__

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char* itoa(int num, char* buf) {
    sprintf(buf, "%d", num);
    return buf;
}

int end_with_slash(const char* str) {
    int len = strlen(str);
    return str[len - 1] == '/';
}

char* remove_slash(char* str) {
    int len = strlen(str);
    if (str[len - 1] == '/') {
        str[len - 1] = '\0';
    }
    return str;
}

char* remove_crlf(char* str) {
    int len = strlen(str);
    if (str[len - 2] == '\r') {
        str[len - 2] = '\0';
    }
    if (str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
    return str;
}

char* path_concat(const char* path1, const char* path2) {
    int end1 = strlen(path1) == 1 && path1[0] == '.' ? 1 : end_with_slash(path1);
    char* result = (char*)malloc(strlen(path1) + strlen(path2) + 2);
    strcpy(result, path1);
    if (!end1 && path2[0] != '/') {
        strcat(result, "/");
    }
    strcat(result, path2);
    // remember to free after concat
    return result;
}

#endif