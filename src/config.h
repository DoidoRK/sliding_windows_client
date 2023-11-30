#ifndef _CONFIG_H_
#define _CONFIG_H_

//Connection settings
#define SERVER_PORT 4000
#define SERVER_IP "192.168.171.67"  // Server's IP Address

//Socket timeout settings
#define SOCKET_TIMEOUT_IN_SECONDS 0
#define SOCKET_TIMEOUT_IN_MICROSSECONDS 10000

//Error simulation settings
#define TIMEOUT_CHANCE_PERCENT  70

//Sliding Windows settings
#define FILE_NAME_SIZE 128
#define WINDOW_SIZE 1
#define CHUNK_SIZE 128 //In Bytes

#define FILE_PATH "/home/doidobr/Projetos/Redes/sliding_windows_client/src/files/"

#endif /* _CONFIG_H_ */