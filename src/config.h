#ifndef _CONFIG_H_
#define _CONFIG_H_

//Connection settings
#define SERVER_PORT 4000

#define SERVER_IP "192.168.171.67"  // Server's IP Address

//Socket timeout settings
#define SOCKET_TIMEOUT_IN_SECONDS 0
#define SOCKET_TIMEOUT_IN_MICROSSECONDS 100000
//If time out time is lesser than
// WINDOW_SIZE * DELAY_BETWEEN_THREAD_CREATION
//Unexpected behaviour may appear

//Error simulation settings
#define ERROR_IN_COMM_CHANCE_PERCENT  10

//Sliding Windows settings
#define WINDOW_SIZE 2
#define FILE_NAME_SIZE 128
#define CHUNK_SIZE 128 //In Bytes
#define DELAY_BETWEEN_THREAD_CREATION 10

#define FILE_PATH "/home/doidobr/Projetos/Redes/sliding_windows_client/src/files/"

#endif /* _CONFIG_H_ */