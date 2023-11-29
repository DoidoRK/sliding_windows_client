#ifndef _SLIDING_WINDOWS_H_
#define _SLIDING_WINDOWS_H_

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"
#include "types.h"
#include "utils/network_utils.h"
#include "utils/time_utils.h"
#include "utils/file_utils.h"

pthread_mutex_t sliding_window_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t current_chunk_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t sliding_window[WINDOW_SIZE];

frame_t *frame_list = NULL;
size_t number_of_chunks = 0;
size_t current_chunk = 0;

using namespace std;

//Upload proccess

//Creates sliding windows threads to deal with file transfer.
// for (uint8_t i = 0; i < WINDOW_SIZE; i++)
// {
//     pthread_create(&sliding_window[i],NULL, fileUploadThread, NULL);
// }

void uploadFile(const char *file_path){
    number_of_chunks = calculateNumChunks(file_path, CHUNK_SIZE);
    loadFramesFromFile(file_path, frame_list, number_of_chunks);

    //Starts upload to server
    int client_socket;
    struct sockaddr_in server_addr;
    operation_packet_t operation_packet, ack_operation_packet;
    socklen_t server_addr_len = sizeof(server_addr);

    operation_packet.ftp_mode = UPLOAD;
    strcpy(operation_packet.file_path, file_path);
    operation_packet.number_of_chunks = number_of_chunks;

    check(
        (client_socket = socket(AF_INET, SOCK_DGRAM, 0)),
        "Failed to create client socket"
    );

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    check(
        (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr)),
        "Failed to set server address.\n"
    );

    struct timeval timeout;      
    timeout.tv_sec = SOCKET_TIMEOUT_IN_SECONDS;
    timeout.tv_usec = SOCKET_TIMEOUT_IN_MICROSSECONDS;

    check(
        (setsockopt (client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof(timeout))),
        "Failed to set socket receive timeout"
    );

    check(
        (sendto(client_socket, &operation_packet, sizeof(operation_packet_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
        "Failed to send operation datagram.\n"
    );

    while ( 0 > recvfrom(client_socket, &ack_operation_packet, sizeof(operation_packet_t), 0, (struct sockaddr*)&server_addr, &server_addr_len))
    {
        cout << "Failed to receive ack from server, retrying connection..." << endl;
        printCurrentTime();
        check(
            (sendto(client_socket, &operation_packet, sizeof(operation_packet_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
            "Failed to send operation datagram.\n"
        );
    }

    cout << "Fazendo Upload de: " << file_path << endl;
    close(client_socket);
}


//Download proccess

//Creates sliding windows threads to deal with file transfer.
// for (uint8_t i = 0; i < WINDOW_SIZE; i++)
// {
//     pthread_create(&sliding_window[i],NULL, fileDownloadThread, NULL);
// }

void downloadFile(const char *file_name){
    cout << "Fazendo Download de: " << file_name << endl;
}

#endif /* _SLIDING_WINDOWS_H_ */