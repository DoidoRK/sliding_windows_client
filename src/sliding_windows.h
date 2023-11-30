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
#include "utils/print_utils.h"

pthread_mutex_t current_chunk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t window_end_chunk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t frame_list_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t sliding_window[WINDOW_SIZE];

frame_t *frame_list = NULL;
int is_running = 0;
size_t number_of_chunks_in_file = 0;
size_t current_chunk = 0, window_end_chunk = 0;

using namespace std;

void loadFramesFromFile(const char file_path[FILE_NAME_SIZE], size_t number_of_chunks_in_file) {
    FILE *file = fopen(file_path, "rb"); // Open the file in binary mode for reading
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Dynamically allocate memory for the frame list
    frame_list = (frame_t *)malloc(number_of_chunks_in_file * sizeof(frame_t));
    if (frame_list == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return;
    }

    // Read frames from the file into the dynamically allocated frame list
    for (size_t i = 0; i < number_of_chunks_in_file; ++i) {
        frame_list[i].status = NOT_ACKNOWLEDGED; // Start frames as not acknowledged

        // Read data from the file into the data property
        size_t bytes_read = fread(frame_list[i].data, sizeof(char), CHUNK_SIZE, file);

        if (bytes_read < CHUNK_SIZE) {
            // Fill the remaining bytes with empty spaces:
            for (size_t j = bytes_read; j < CHUNK_SIZE; ++j) {
                frame_list[i].data[j] = 32;
            }
        }
    }

    fclose(file); // Close the file after reading
}

//Upload proccess
void* uploadFileThread(void* arg){
    uint16_t thread_port = *((uint16_t*)arg);
    int thread_socket;
    struct sockaddr_in server_addr;
    data_packet_t data_packet, ack_packet;
    socklen_t server_addr_len = sizeof(server_addr);

    check(
        (thread_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)),
        "Failed to create server's socket.\n"
    );

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(thread_port);
    check(
        (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr)),
        "Failed to set server address.\n"
    );

    struct timeval timeout;
    timeout.tv_sec = SOCKET_TIMEOUT_IN_SECONDS;
    timeout.tv_usec = SOCKET_TIMEOUT_IN_MICROSSECONDS;
    check(
        (setsockopt (thread_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))),
        "Failed to set upload thread socket receive timeout"
    );

    // Use select to detect timeout
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(thread_socket, &readSet);

    while (current_chunk < number_of_chunks_in_file)
    {
        if(window_end_chunk - current_chunk < WINDOW_SIZE) {
            if(window_end_chunk < number_of_chunks_in_file){
                data_packet.sequence_number = window_end_chunk;
                pthread_mutex_lock(&frame_list_mutex);
                memcpy(&(data_packet.frame), &(frame_list[window_end_chunk]), sizeof(frame_t));
                pthread_mutex_lock(&window_end_chunk_mutex);
                window_end_chunk++;
                pthread_mutex_unlock(&window_end_chunk_mutex);
                pthread_mutex_unlock(&frame_list_mutex);
                if(NOT_ACKNOWLEDGED == data_packet.frame.status){
                    printDataPacket(current_chunk, number_of_chunks_in_file, thread_port, data_packet, SEND_DATA_PACKET);
                    check(
                        (sendto(thread_socket, &data_packet, sizeof(data_packet_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
                        "Upload thread failed to send data packet.\n"
                    );
                    // Data is ready to be received
                    int recv_result = recvfrom(thread_socket, &ack_packet, sizeof(data_packet_t), 0, (struct sockaddr*)&server_addr, &server_addr_len);
                    if (recv_result < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // Timeout occurred
                            printTimeOutError(data_packet.sequence_number);
                        } else {
                            perror("Error receiving data");
                        }
                        pthread_mutex_lock(&window_end_chunk_mutex);
                        //If there is a error receiving package ack, go back N
                        window_end_chunk = current_chunk;
                        pthread_mutex_unlock(&window_end_chunk_mutex);
                    } else {
                        printDataPacket(current_chunk,number_of_chunks_in_file, thread_port, ack_packet, RECV_DATA_PACKET);
                        if(ACKNOWLEDGED == ack_packet.frame.status){
                            pthread_mutex_lock(&frame_list_mutex);
                            frame_list[window_end_chunk].status = ACKNOWLEDGED;
                            pthread_mutex_lock(&current_chunk_mutex);
                            current_chunk++;
                            pthread_mutex_unlock(&current_chunk_mutex);
                            pthread_mutex_unlock(&frame_list_mutex);
                        }
                    }
                } else {
                    pthread_mutex_lock(&current_chunk_mutex);
                    current_chunk++;
                    pthread_mutex_unlock(&current_chunk_mutex);
                }
            }
        }
    }
    is_running = 0;
    return 0;
}

void uploadFile(string file_name){
    is_running = 1;
    string file_path = FILE_PATH + file_name;
    cout << file_path << endl;
    number_of_chunks_in_file = calculateNumChunks(file_path.c_str(), CHUNK_SIZE);
    cout << "Quantidade de Chunks " << number_of_chunks_in_file << endl;
    loadFramesFromFile(file_path.c_str(), number_of_chunks_in_file);
    cout << "Carregou arquivo no Buffer" << endl;

    //Starts upload to server
    int client_socket;
    struct sockaddr_in server_addr;
    operation_packet_t operation_packet, ack_operation_packet;
    socklen_t server_addr_len = sizeof(server_addr);

    operation_packet.ftp_mode = UPLOAD;
    strcpy(operation_packet.file_name,file_name.c_str());
    operation_packet.number_of_chunks_in_file = number_of_chunks_in_file;

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
        (setsockopt (client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))),
        "Failed to set socket receive timeout"
    );

    check(
        (sendto(client_socket, &operation_packet, sizeof(operation_packet_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
        "Failed to send operation datagram.\n"
    );

    while ( 0 > recvfrom(client_socket, &ack_operation_packet, sizeof(operation_packet_t), 0, (struct sockaddr*)&server_addr, &server_addr_len))
    {
        cout << "Failed to receive operation ack from server, retrying connection..." << endl;
        printCurrentTime();
        check(
            (sendto(client_socket, &operation_packet, sizeof(operation_packet_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
            "Failed to send operation datagram.\n"
        );
    }

    //Creates server packets to handle data packets
    for (size_t i = 0; i < WINDOW_SIZE; i++)
    {
        uint16_t thread_port = SERVER_PORT + 1 + i;
        if (pthread_create(&sliding_window[i], NULL, uploadFileThread, (void*)&thread_port) != 0) {
            perror("Error creating download thread.\n");
            return;
        }
        usleep(DELAY_BETWEEN_THREAD_CREATION);
    }
    while (is_running); 
    close(client_socket);
}


//Download proccess
void downloadFile(const char *file_name){
    cout << "Fazendo Download de: " << file_name << endl;
}

#endif /* _SLIDING_WINDOWS_H_ */