#ifndef __FTP_TYPES__
#define __FTP_TYPES__

enum LOG_TYPE {
    LOG_TYPE_NONE = 0,
    LOG_TYPE_ERRO = 1,
    LOG_TYPE_WARN = 2,
    LOG_TYPE_INFO = 3,
    LOG_TYPE_DBUG = 4,
    LOG_TYPE_TRCE = 5,
    LOG_TYPE_RECV = 6
};

enum LOGIN_STATUS {
    LOGIN_STATUS_NONE = 0,
    LOGIN_STATUS_USER = 1,
    LOGIN_STATUS_LGIN = 3
};

enum TR_MODE {
    TR_MODE_NONE = 0,
    TR_MODE_PORT = 1,
    TR_MODE_PASV = 2
};

#endif