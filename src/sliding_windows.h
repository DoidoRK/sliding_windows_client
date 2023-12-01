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

pthread_mutex_t current_index_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t window_end_index_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t frame_list_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t sliding_window[WINDOW_SIZE];

frame_t *frame_list = NULL;
int is_running = 0;
size_t frame_list_last_index = 0;
size_t current_frame_index = 0, window_end_index = 0;

using namespace std;

void loadFramesFromFile(const char file_path[FILE_NAME_SIZE], size_t frame_list_last_index) {
    FILE *file = fopen(file_path, "rb"); // Open the file in binary mode for reading
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Dynamically allocate memory for the frame list
    frame_list = (frame_t *)malloc(frame_list_last_index * sizeof(frame_t));
    if (frame_list == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return;
    }

    // Read frames from the file into the dynamically allocated frame list
    for (size_t i = 0; i < frame_list_last_index; ++i) {
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
    int thread_socket, thread_status = OPERATION_IN_BUFFER;
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

    int recv_result, send_success_chance;
    while (current_frame_index < frame_list_last_index)
    {
        cout << "Current Index: " << current_frame_index << " Window End: " << window_end_index << endl;
        switch (thread_status)
        {
            case OPERATION_IN_BUFFER:
                if(window_end_index < frame_list_last_index){
                    if(frame_list[current_frame_index].status == NOT_ACKNOWLEDGED){
                        if(window_end_index - current_frame_index < WINDOW_SIZE) {
                            data_packet.sequence_number = window_end_index;
                            pthread_mutex_lock(&frame_list_mutex);
                            memcpy(&(data_packet.frame), &(frame_list[window_end_index]), sizeof(frame_t));
                            pthread_mutex_lock(&window_end_index_mutex);
                            window_end_index++;
                            pthread_mutex_unlock(&window_end_index_mutex);
                            pthread_mutex_unlock(&frame_list_mutex);
                        }
                        thread_status = SENDING_DATA;
                    } else {
                        thread_status = WAITING_FOR_DATA;
                    }
                }
                break;

            case SENDING_DATA:
                printDataPacket(frame_list_last_index, thread_port, data_packet, SEND_DATA_PACKET);
                send_success_chance = generateRandomNumber();
                if(send_success_chance > ERROR_IN_COMM_CHANCE_PERCENT){
                    check(
                        (sendto(thread_socket, &data_packet, sizeof(data_packet_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
                        "Upload thread failed to send data packet.\n"
                    );
                } else {
                    printSendError(thread_port, data_packet.sequence_number);
                }
                thread_status = WAITING_FOR_DATA;
                break;


            case WAITING_FOR_DATA:
                recv_result = recvfrom(thread_socket, &ack_packet, sizeof(data_packet_t), 0, (struct sockaddr*)&server_addr, &server_addr_len);
                if (recv_result > 0) {
                    printDataPacket(frame_list_last_index, thread_port, ack_packet, RECV_DATA_PACKET);
                    if (ack_packet.sequence_number == current_frame_index)
                    {
                        pthread_mutex_lock(&current_index_mutex);
                        current_frame_index++;
                        pthread_mutex_unlock(&current_index_mutex);
                    }
                    pthread_mutex_lock(&frame_list_mutex);
                    frame_list[ack_packet.sequence_number].status = ACKNOWLEDGED;
                    pthread_mutex_unlock(&frame_list_mutex);
                    thread_status = OPERATION_IN_BUFFER;
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Timeout occurred
                        printAckTimeOutError(thread_port, data_packet.sequence_number);
                    } else {
                        perror("Error receiving data");
                    }
                    pthread_mutex_lock(&window_end_index_mutex);
                    pthread_mutex_lock(&current_index_mutex);
                    //If there is a error receiving package ack, go back N
                    window_end_index = current_frame_index;
                    pthread_mutex_unlock(&current_index_mutex);
                    pthread_mutex_unlock(&window_end_index_mutex);
                }
                break;

            
            default:    //In Upload threads, default state is waiting for data
                cout << "Thread Status desconhecido";
                break;
        }
    }
    is_running = 0;
    return 0;
}

void uploadFile(string file_name){
    is_running = 1;
    string file_path = FILE_PATH + file_name;
    cout << file_path << endl;
    frame_list_last_index = calculateNumChunks(file_path.c_str(), CHUNK_SIZE);
    cout << "Quantidade de Chunks " << frame_list_last_index << endl;
    loadFramesFromFile(file_path.c_str(), frame_list_last_index);
    cout << "Carregou arquivo no Buffer" << endl;

    //Starts upload to server
    int client_socket;
    struct sockaddr_in server_addr;
    operation_packet_t operation_packet, ack_operation_packet;
    socklen_t server_addr_len = sizeof(server_addr);

    operation_packet.ftp_mode = UPLOAD;
    strcpy(operation_packet.file_name,file_name.c_str());
    operation_packet.file_size_in_chunks = frame_list_last_index;

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