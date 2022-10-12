#ifndef __FTP_NETWORK__
#define __FTP_NETWORK__

#include "logs.hpp"
#include "types.hpp"
#include "config.hpp"
#include "file.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>

#include <pthread.h>

extern int MAX_CLIENTS;
extern int current_clients;

struct sockaddr_in server_addr;
int open_main(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        logs(LOG_TYPE_ERRO, "Failed to create socket. Error: %s, %d", strerror(errno), errno);
        return -1;
    }
	memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        logs(LOG_TYPE_ERRO, "Failed to bind socket. Error: %s, %d", strerror(errno), errno);
        return -1;
    }
    if (listen(sock, 10) < 0) {
        logs(LOG_TYPE_ERRO, "Failed to listen socket. Error: %s, %d", strerror(errno), errno);
        return -1;
    }
    return sock;
}

struct ftp_connection {
	int connfd;
	struct sockaddr_in* client_addr;
	pthread_t tid;
};

void close_conn(int pasvfd, int datafd) {
	if (pasvfd != -1) {
		shutdown(pasvfd, SHUT_RDWR);
		close(pasvfd);
	}
	if (datafd != -1) {
		shutdown(datafd, SHUT_RDWR);
		close(datafd);
	}
}

int send_data_by_mode(int* pasvfd, int* datafd, char* buffer, int bufsize, enum TR_MODE mode, struct sockaddr_in* data_addr, int* connected) {
	if (mode == TR_MODE_PASV) {
		if (*datafd == -1) {
			socklen_t len = sizeof(struct sockaddr_in);
			*datafd = accept(*pasvfd, (struct sockaddr*)data_addr, &len);
		}
		return send(*datafd, buffer, bufsize, 0);
	} else if (mode == TR_MODE_PORT) {
		if (!(*connected)) {
			if (connect(*datafd, (struct sockaddr*)data_addr, sizeof(*data_addr)) < 0) {
				logs(LOG_TYPE_ERRO, "Failed to connect to socket. Error: %s, %d", strerror(errno), errno);
				return -1;
			}
			*connected = 1;
		}
		return send(*datafd, buffer, bufsize, 0);
	}
	return -1;
}

int recv_data_by_mode(int* pasvfd, int* datafd, char* buffer, int bufsize, enum TR_MODE mode, struct sockaddr_in* data_addr, int* connected) {
	if (mode == TR_MODE_PASV) {
		if (*datafd == -1) {
			socklen_t len = sizeof(struct sockaddr_in);
			logs(LOG_TYPE_INFO, "Waiting for data connection...");
			*datafd = accept(*pasvfd, (struct sockaddr*)data_addr, &len);
			logs(LOG_TYPE_INFO, "Data connection established.");
		}
		return recv(*datafd, buffer, bufsize, 0);
	} else if (mode == TR_MODE_PORT) {
		if (!(*connected)) {
			if (connect(*datafd, (struct sockaddr*)data_addr, sizeof(*data_addr)) < 0) {
				logs(LOG_TYPE_ERRO, "Failed to connect to socket. Error: %s, %d", strerror(errno), errno);
				return -1;
			}
			*connected = 1;
		}
		return recv(*datafd, buffer, bufsize, 0);
	}
	return -1;
}

void* ftp_connection_thread(struct ftp_connection* f_conn) {
	if (current_clients >= MAX_CLIENTS) {
		logs(LOG_TYPE_ERRO, "Max clients reached");
		send(f_conn->connfd, "421 Too many clients\r\n", 22, 0);
		close(f_conn->connfd);
		free(f_conn->client_addr);
		free(f_conn);
		pthread_exit(NULL);
		return NULL;
	}
	current_clients++;
	enum LOGIN_STATUS login_status = LOGIN_STATUS_NONE;
	enum TR_MODE tr_mode = TR_MODE_NONE;
	int pasv_sock = -1;
	struct sockaddr_in pasv_addr;
	int data_sock = -1;
	struct sockaddr_in data_addr;
	int connected = 0;
	logs(LOG_TYPE_INFO, "Current clients: %d", current_clients);
	send(f_conn->connfd, "220 Almost Harmless FTP Server\r\n", 32, 0);
	char* curpath = (char*)malloc(256);
	memset(curpath, 0, 256);
	curpath[0] = '/';
	char* rnfr_path = (char*)malloc(256);
	memset(rnfr_path, 0, 256);
	rnfr_path[0] = '/';
	char* buffer = (char*)malloc(size + 1);
	char* databuffer = (char*)malloc(size + 1);
	while (1) {
		memset(buffer, 0, size);
		memset(databuffer, 0, size);
		int rc = recv(f_conn->connfd, buffer, size, 0);
		if (rc < 0) {
			logs(LOG_TYPE_ERRO, "Failed to receive data. Error: %s, %d", strerror(errno), errno);
			break;
		} else if (rc == 0) {
			logs(LOG_TYPE_INFO, "Connection normally closed");
			break;
		}
		buffer[rc] = '\0';
		logs(LOG_TYPE_RECV, buffer);
		if (strncmp(buffer, "USER", 4) == 0) {
			if (strncmp(buffer + 5, "anonymous", 9) == 0) {
				send(f_conn->connfd, "331 Anonymous login ok, send your email address as password\r\n", 61, 0);
				login_status = LOGIN_STATUS_USER;
				close_conn(pasv_sock, data_sock);
				pasv_sock = -1;
				data_sock = -1;
				tr_mode = TR_MODE_NONE;
				connected = 0;
				memset(curpath, 0, 256);
				curpath[0] = '/';
				memset(rnfr_path, 0, 256);
				rnfr_path[0] = '/';
			} else {
				send(f_conn->connfd, "530 Currently only anonymous login is supported\r\n", 49, 0);
			}
		} else if (strncmp(buffer, "PASS", 4) == 0) {
			if (login_status == LOGIN_STATUS_USER) {
				send(f_conn->connfd, "230 Login successful\r\n", 22, 0);
				login_status = LOGIN_STATUS_LGIN;
			} else {
				send(f_conn->connfd, "503 Login with USER first\r\n", 27, 0);
			}
		} else if (strncmp(buffer, "SYST", 4) == 0) {
			send(f_conn->connfd, "215 UNIX Type: L8\r\n", 19, 0);
		} else if (strncmp(buffer, "TYPE", 4) == 0) {
			if (buffer[5] == 'I') {
				send(f_conn->connfd, "200 Type set to I.\r\n", 20, 0);
			} else {
				send(f_conn->connfd, "504 Only Type I supported\r\n", 27, 0);
			}
		} else if (strncmp(buffer, "QUIT", 4) == 0 || strncmp(buffer, "ABOR", 4) == 0) {
			send(f_conn->connfd, "221 Goodbye\r\n", 13, 0);
			break;
		} else if (login_status != LOGIN_STATUS_LGIN) {
			send(f_conn->connfd, "530 Please login with USER and PASS\r\n", 37, 0);
		} else if (strncmp(buffer, "PASV", 4) == 0) {
			close_conn(pasv_sock, data_sock);
			pasv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (pasv_sock < 0) {
				logs(LOG_TYPE_ERRO, "Failed to create socket. Error: %s, %d", strerror(errno), errno);
				send(f_conn->connfd, "425 Failed to create socket\r\n", 29, 0);
				continue;
			}
			memset(&pasv_addr, 0, sizeof(pasv_addr));
			pasv_addr.sin_family = AF_INET;
			pasv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
			pasv_addr.sin_port = htons(0);
			if (bind(pasv_sock, (struct sockaddr*)&pasv_addr, sizeof(pasv_addr)) < 0) {
				logs(LOG_TYPE_ERRO, "Failed to bind socket. Error: %s, %d", strerror(errno), errno);
				send(f_conn->connfd, "425 Failed to bind socket\r\n", 27, 0);
				close(pasv_sock);
				pasv_sock = -1;
				continue;
			}
			if (listen(pasv_sock, 1) < 0) {
				logs(LOG_TYPE_ERRO, "Failed to listen on socket. Error: %s, %d", strerror(errno), errno);
				send(f_conn->connfd, "425 Failed to listen on socket\r\n", 32, 0);
				close(pasv_sock);
				pasv_sock = -1;
				continue;
			}
			socklen_t pasv_addr_len = sizeof(pasv_addr);
			if (getsockname(pasv_sock, (struct sockaddr*)&pasv_addr, &pasv_addr_len) < 0) {
				logs(LOG_TYPE_ERRO, "Failed to get socket name. Error: %s, %d", strerror(errno), errno);
				send(f_conn->connfd, "425 Failed to get socket name\r\n", 31, 0);
				close(pasv_sock);
				pasv_sock = -1;
				continue;
			}
			char* ip = inet_ntoa(pasv_addr.sin_addr);
			for (size_t i = 0; i < strlen(ip); i++) {
				if (ip[i] == '.') {
					ip[i] = ',';
				}
			}
			int port = ntohs(pasv_addr.sin_port);
			char* resp = (char*)malloc(128);
			sprintf(resp, "227 Entering Passive Mode (%s,%d,%d)\r\n", ip, port / 256, port % 256);
			logs(LOG_TYPE_INFO, "PASV response: %s", resp);
			send(f_conn->connfd, resp, strlen(resp), 0);
			free(resp);
			tr_mode = TR_MODE_PASV;
		} else if (strncmp(buffer, "PORT", 4) == 0) {
			close_conn(pasv_sock, data_sock);
			char* ip = (char*)malloc(16);
			memset(ip, 0, 16);
			char* pos = strtok(buffer + 5, ",");
			int flag = 0;
			for (size_t i = 0; i < 4; i++) {
				if (pos == NULL) {
					send(f_conn->connfd, "501 Invalid PORT command\r\n", 26, 0);
					free(ip);
					flag = 1;
					break;
				}
				strcat(ip, pos);
				if (i != 3) {
					strcat(ip, ".");
				}
				pos = strtok(NULL, ",");
			}
			int port_num = 0;
			for (size_t i = 0; i < 2; i++) {
				if (pos == NULL) {
					send(f_conn->connfd, "501 Invalid PORT command\r\n", 26, 0);
					free(ip);
					flag = 1;
					break;
				}
				port_num = port_num * 256 + atoi(pos);
				pos = strtok(NULL, ",");
			}
			if (flag) {
				logs(LOG_TYPE_ERRO, "Invalid PORT command");
				continue;
			}
			logs(LOG_TYPE_INFO, "PORT IP: %s %d, PORT: %d", ip, strlen(ip), port_num);
			data_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (data_sock < 0) {
				logs(LOG_TYPE_ERRO, "Failed to create socket. Error: %s, %d", strerror(errno), errno);
				send(f_conn->connfd, "425 Failed to create socket\r\n", 29, 0);
			}
			memset(&data_addr, 0, sizeof(data_addr));
			data_addr.sin_family = AF_INET;
			data_addr.sin_addr.s_addr = inet_addr(ip);
			data_addr.sin_port = htons(port_num);
			send(f_conn->connfd, "200 PORT command successful\r\n", 29, 0);
			tr_mode = TR_MODE_PORT;
		} else if (strncmp(buffer, "LIST", 4) == 0 || strncmp(buffer, "NLST", 4) == 0) {
			if (tr_mode == TR_MODE_NONE) {
				logs(LOG_TYPE_ERRO, "No transfer mode specified");
				send(f_conn->connfd, "425 Use PORT or PASV first\r\n", 28, 0);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 5);
			remove_crlf(path);
			logs(LOG_TYPE_INFO, "LIST path: %s", path);
			char* res = list_dir(path, buffer[0] == 'L', databuffer);
			if (res != NULL) {
				send(f_conn->connfd, "150 Here comes the directory listing\r\n", 38, 0);
				int r = send_data_by_mode(&pasv_sock, &data_sock, res, strlen(res), tr_mode, &data_addr, &connected);
				if (r >= 0) {
					logs(LOG_TYPE_INFO, "LIST/NLST success, sent %d bytes", r);
					char* resp = (char*)malloc(128);
					sprintf(resp, "226 Directory send OK, %d bytes\r\n", r);
					send(f_conn->connfd, resp, strlen(resp), 0);
					free(resp);
				} else {
					logs(LOG_TYPE_ERRO, "LIST/NLST failed due to connection error");
					send(f_conn->connfd, "425 Failed to send directory\r\n", 30, 0);
				}
			} else {
				logs(LOG_TYPE_ERRO, "LIST/NLST failed: no such directory, permission denied or too long");
				send(f_conn->connfd, "550 Failed to list directory\r\n", 30, 0);
			}
			close_conn(pasv_sock, data_sock);
			pasv_sock = -1;
			data_sock = -1;
			tr_mode = TR_MODE_NONE;
			connected = 0;
			free(curpath_full);
			free(path);
		} else if (strncmp(buffer, "PWD", 3) == 0) {
			char* resp = (char*)malloc(256);
			sprintf(resp, "257 \"%s\" is current directory\r\n", curpath);
			send(f_conn->connfd, resp, strlen(resp), 0);
		} else if (strncmp(buffer, "CWD", 3) == 0) {
			if (buffer[3] != ' ') {
				send(f_conn->connfd, "501 Invalid CWD command\r\n", 25, 0);
				continue;
			}
			if (buffer[4] == '.') {
				send(f_conn->connfd, "550 Please do not use . in path\r\n", 33, 0);
				continue;
			}
			if (buffer[4] == '/') {
				char* new_path = path_concat(ROOT, buffer + 4);
				remove_crlf(new_path);
				logs(LOG_TYPE_INFO, "CWD absolute path: %s", new_path);
				if (check_dir(new_path)) {
					strcpy(curpath, buffer + 4);
					remove_crlf(curpath);
					send(f_conn->connfd, "250 Directory successfully changed\r\n", 36, 0);
					rnfr_path[0] = '\0';
				} else {
					send(f_conn->connfd, "550 Failed to change directory\r\n", 32, 0);
				}
				free(new_path);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 4);
			remove_crlf(path);
			if (check_dir(path)) {
				strcpy(curpath, path + strlen(ROOT));
				send(f_conn->connfd, "250 Directory successfully changed\r\n", 36, 0);
			} else {
				send(f_conn->connfd, "550 Failed to change directory\r\n", 32, 0);
			}
			free(curpath_full);
			free(path);
		} else if (strncmp(buffer, "MKD", 3) == 0) {
			if (buffer[3] != ' ') {
				send(f_conn->connfd, "501 Invalid MKD command\r\n", 25, 0);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 4);
			remove_crlf(path);
			if (mkdir(path, 0777) == 0) {
				send(f_conn->connfd, "257 Directory created\r\n", 23, 0);
			} else {
				send(f_conn->connfd, "550 Failed to create directory\r\n", 32, 0);
			}
			free(curpath_full);
			free(path);
		} else if (strncmp(buffer, "RMD", 3) == 0) {
			if (buffer[3] != ' ') {
				send(f_conn->connfd, "501 Invalid RMD command\r\n", 25, 0);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 4);
			remove_crlf(path);
			if (rmdir(path) == 0) {
				send(f_conn->connfd, "250 Directory removed\r\n", 23, 0);
				rnfr_path[0] = '\0';
			} else {
				send(f_conn->connfd, "550 Failed to remove directory\r\n", 32, 0);
			}
			free(curpath_full);
			free(path);
		} else if (strncmp(buffer, "DELE", 4) == 0) {
			if (buffer[4] != ' ') {
				send(f_conn->connfd, "501 Invalid DELE command\r\n", 26, 0);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 5);
			remove_crlf(path);
			if (unlink(path) == 0) {
				send(f_conn->connfd, "250 File removed\r\n", 18, 0);
				rnfr_path[0] = '\0';
			} else {
				send(f_conn->connfd, "550 Failed to remove file\r\n", 27, 0);
			}
			free(curpath_full);
			free(path);
		} else if (strncmp(buffer, "RNFR", 4) == 0) {
			if (buffer[4] != ' ') {
				send(f_conn->connfd, "501 Invalid RNFR command\r\n", 26, 0);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 5);
			remove_crlf(path);
			if (check_file(path) || check_dir(path)) {
				strcpy(rnfr_path, path);
				send(f_conn->connfd, "350 Ready for RNTO\r\n", 20, 0);
			} else {
				send(f_conn->connfd, "550 File or directory not found\r\n", 33, 0);
			}
			free(curpath_full);
			free(path);
		} else if (strncmp(buffer, "RNTO", 4) == 0) {
			if (buffer[4] != ' ') {
				send(f_conn->connfd, "501 Invalid RNTO command\r\n", 26, 0);
				continue;
			}
			if (strlen(rnfr_path) == 0) {
				send(f_conn->connfd, "503 RNFR required first\r\n", 25, 0);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 5);
			remove_crlf(path);
			if (rename(rnfr_path, path) == 0) {
				send(f_conn->connfd, "250 File or directory renamed\r\n", 31, 0);
				rnfr_path[0] = '\0';
			} else {
				send(f_conn->connfd, "550 Failed to rename file or directory\r\n", 39, 0);
			}
			free(curpath_full);
			free(path);
		} else if (strncmp(buffer, "RETR", 4) == 0) {
			if (buffer[4] != ' ') {
				send(f_conn->connfd, "501 Invalid RETR command\r\n", 26, 0);
				continue;
			}
			if (tr_mode == TR_MODE_NONE) {
				send(f_conn->connfd, "425 Use PORT or PASV first\r\n", 28, 0);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 5);
			remove_crlf(path);
			if (check_file_read(path)) {
				send(f_conn->connfd, "150 Opening BINARY mode data connection\r\n", 41, 0);
				FILE* f = fopen(path, "rb");
				if (f == NULL) {
					send(f_conn->connfd, "550 Failed to open file\r\n", 25, 0);
				} else {
					int read_size = fread(databuffer, 1, size, f);
					while (read_size > 0) {
						logs(LOG_TYPE_INFO, "Sending %d bytes", read_size);
						send_data_by_mode(&pasv_sock, &data_sock, databuffer, read_size, tr_mode, &data_addr, &connected);
						// fseek(f, read_size, SEEK_CUR);
						read_size = fread(databuffer, 1, size, f);
					}
					fclose(f);
					logs(LOG_TYPE_INFO, "File sent");
					send(f_conn->connfd, "226 Transfer complete\r\n", 23, 0);
				}
			} else {
				send(f_conn->connfd, "550 File not found or permission denied\r\n", 41, 0);
			}
			free(curpath_full);
			free(path);
			tr_mode = TR_MODE_NONE;
			connected = 0;
			close_conn(data_sock, pasv_sock);
			data_sock = -1;
			pasv_sock = -1;
		} else if (strncmp(buffer, "STOR", 4) == 0) {
			if (buffer[4] != ' ') {
				send(f_conn->connfd, "501 Invalid STOR command\r\n", 26, 0);
				continue;
			}
			if (tr_mode == TR_MODE_NONE) {
				send(f_conn->connfd, "425 Use PORT or PASV first\r\n", 28, 0);
				continue;
			}
			char* curpath_full = path_concat(ROOT, curpath);
			char* path = path_concat(curpath_full, buffer + 5);
			remove_crlf(path);
			FILE* f = fopen(path, "wb");
			if (f == NULL) {
				send(f_conn->connfd, "550 Failed to open file or permission denied\r\n", 25, 0);
			} else {
				send(f_conn->connfd, "150 Opening BINARY mode data connection\r\n", 41, 0);
				int read_size = recv_data_by_mode(&pasv_sock, &data_sock, databuffer, size, tr_mode, &data_addr, &connected);
				while (read_size > 0) {
					logs(LOG_TYPE_INFO, "Receiving %d bytes", read_size);
					fwrite(databuffer, 1, read_size, f);
					read_size = recv_data_by_mode(&pasv_sock, &data_sock, databuffer, size, tr_mode, &data_addr, &connected);
				}
				fclose(f);
				logs(LOG_TYPE_INFO, "File received");
				send(f_conn->connfd, "226 Transfer complete\r\n", 23, 0);
			}
			free(curpath_full);
			free(path);
			tr_mode = TR_MODE_NONE;
			connected = 0;
			close_conn(data_sock, pasv_sock);
			data_sock = -1;
			pasv_sock = -1;
		} else {
			send(f_conn->connfd, "500 Unknown command\r\n", 21, 0);
		}
	}
	free(buffer);
	free(databuffer);
	free(curpath);
	current_clients--;
	logs(LOG_TYPE_INFO, "Connection closed at: %s:%d", inet_ntoa(f_conn->client_addr->sin_addr), ntohs(f_conn->client_addr->sin_port));
	logs(LOG_TYPE_INFO, "Current clients: %d", current_clients);
	close_conn(pasv_sock, data_sock);
	close(f_conn->connfd);
	free(f_conn->client_addr);
	free(f_conn);
	pthread_exit(NULL);
}

#endif