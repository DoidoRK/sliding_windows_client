#ifndef _CONFIG_H_
#define _CONFIG_H_

//Connection settings
#define SERVER_PORT 4000
#define SERVER_IP "192.168.171.67"  // Server's IP Address

//Socket timeout settings
#define SOCKET_TIMEOUT_IN_SECONDS 2
#define SOCKET_TIMEOUT_IN_MICROSSECONDS 0


//Sliding Windows settings
#define FILE_NAME_SIZE 128
#define OPERATION_BUFFER_SIZE 9
#define WINDOW_SIZE 1
#define CHUNK_SIZE 512 //In Bytes

#define FILE_PATH "/home/doidobr/Projetos/Redes/sliding_windows_client/src/files/"

#endif /* _CONFIG_H_ */