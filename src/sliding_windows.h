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

pthread_mutex_t current_frame_index_mutex = PTHREAD_MUTEX_INITIALIZER;
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
    size_t thread_num = *((size_t*)arg);
    uint16_t thread_port = SERVER_PORT + 1 + thread_num;
    int thread_socket, thread_status = READ_WRITE_FRAME_BUFFER;
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

    struct timeval timeout;
    timeout.tv_sec = SOCKET_TIMEOUT_IN_SECONDS;
    timeout.tv_usec = SOCKET_TIMEOUT_IN_MICROSSECONDS;
    check(
        (setsockopt (thread_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))),
        "Failed to set upload thread socket receive timeout"
    );

    int recv_result, comm_success_chance;
    size_t thread_data_package_index;
    while (current_frame_index < frame_list_last_index)
    {
        pthread_mutex_lock(&current_frame_index_mutex);
        thread_data_package_index = current_frame_index+thread_num;
        pthread_mutex_unlock(&current_frame_index_mutex);
        pthread_mutex_lock(&window_end_index_mutex);
        window_end_index = current_frame_index+WINDOW_SIZE;
        pthread_mutex_unlock(&window_end_index_mutex);
        switch (thread_status)
        {
        case READ_WRITE_FRAME_BUFFER:
            if(thread_data_package_index < frame_list_last_index){
                data_packet.sequence_number = thread_data_package_index;
                pthread_mutex_lock(&frame_list_mutex);
                memcpy(&(data_packet.frame), &(frame_list[thread_data_package_index]), sizeof(frame_t));
                pthread_mutex_unlock(&frame_list_mutex);
                thread_status = SENDING_DATA;
            }
            break;

        case SENDING_DATA:
            printDataPacket(current_frame_index, window_end_index, frame_list_last_index, thread_port, data_packet, SEND_DATA_PACKET);
            comm_success_chance = generateRandomNumber();
            if(comm_success_chance > CHANCE_FOR_ERROR_IN_SEND_PERCENT){
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
                printDataPacket(current_frame_index, window_end_index, frame_list_last_index, thread_port, ack_packet, RECV_DATA_PACKET);
                if (ack_packet.sequence_number == (thread_data_package_index) && (thread_data_package_index) == current_frame_index)
                {
                    pthread_mutex_lock(&frame_list_mutex);
                    frame_list[current_frame_index].status = ACKNOWLEDGED;
                    pthread_mutex_unlock(&frame_list_mutex);
                    thread_status = NEXT_INDEX;
                }
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Timeout occurred
                    printAckTimeOutError(thread_port, data_packet.sequence_number);
                } else {
                    perror("Error receiving data");
                }
                thread_status = ACK_FAILED;
            }
            break;

        case NEXT_INDEX:
            //Only the thread for the first element in the window can move the window
            if(thread_num == 0){
                pthread_mutex_lock(&current_frame_index_mutex);
                current_frame_index++;
                pthread_mutex_unlock(&current_frame_index_mutex);
            }
            thread_status = READ_WRITE_FRAME_BUFFER;
            break;

        case ACK_FAILED:
            if(data_packet.sequence_number < current_frame_index){
                pthread_mutex_lock(&current_frame_index_mutex);
                //If there is a error receiving package ack, go back N
                current_frame_index = data_packet.sequence_number;
                pthread_mutex_unlock(&current_frame_index_mutex);
                //And reset every frame from there to NOT_ACKNOWLEDGED
                size_t i = data_packet.sequence_number;
                while (frame_list[i].status == ACKNOWLEDGED)
                {
                    pthread_mutex_lock(&frame_list_mutex);
                    frame_list[i].status = NOT_ACKNOWLEDGED;
                    pthread_mutex_unlock(&frame_list_mutex);
                    i++;
                }
            }
            //Repeats proccess to read data and send it.
            thread_status = READ_WRITE_FRAME_BUFFER;
            break;
        
        default:
            cout << "Upload Thread: " << thread_port << "Unknown Thread status" << thread_status;
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
        if (pthread_create(&sliding_window[i], NULL, uploadFileThread, (void*)&i) != 0) {
            perror("Error creating download thread.\n");
            return;
        }
        usleep(DELAY_BETWEEN_THREAD_CREATION);
    }
    while (is_running); 
    close(client_socket);
}


//Download proccess
void writeFileFromFrames(const char file_path[FILE_NAME_SIZE], size_t frame_list_last_index) {
    FILE *file = fopen(file_path, "wb"); // Open the file in binary mode for writing
    if (file == NULL) {
        perror("Error opening file for writing");
        return;
    }

    // Write frames from the frame list to the file
    for (size_t i = 0; i < frame_list_last_index; ++i) {
        size_t bytesWritten = fwrite(frame_list[i].data, sizeof(char), CHUNK_SIZE, file);

        if (bytesWritten < CHUNK_SIZE) {
            perror("Error writing to file");
            fclose(file);
            return;
        }
    }

    fclose(file); // Close the file after writing
}

void* downloadFileThread(void* arg){
    size_t thread_num = *((size_t*)arg);
    uint16_t thread_port = SERVER_PORT + 1 + thread_num;
    int thread_socket, thread_status = WAITING_FOR_DATA;

    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    data_packet_t recv_packet, ack_packet;

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

    check(
        (bind(thread_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))),
        "Failed to bind download thread socket.\n"
    );

    int send_ack_success_chance, comm_success_chance, recv_result;
    size_t thread_data_package_index;
    while (is_running)
    {
        pthread_mutex_lock(&current_frame_index_mutex);
        thread_data_package_index = current_frame_index+thread_num;
        pthread_mutex_unlock(&current_frame_index_mutex);
        pthread_mutex_lock(&window_end_index_mutex);
        window_end_index = current_frame_index+WINDOW_SIZE;
        pthread_mutex_unlock(&window_end_index_mutex);
        switch (thread_status)
        {
            case WAITING_FOR_DATA:
                if(frame_list_last_index > current_frame_index && (frame_list_last_index + WINDOW_SIZE + 2) > window_end_index){
                    comm_success_chance = generateRandomNumber();
                    if(comm_success_chance > CHANCE_FOR_ERROR_IN_RECV_PERCENT){
                        recv_result = recvfrom(thread_socket, &recv_packet, sizeof(data_packet_t), 0, (struct sockaddr*)&server_addr, &server_addr_len);
                        if (recv_result > 0) {
                            printDataPacket(current_frame_index, window_end_index, frame_list_last_index, thread_port, recv_packet, RECV_DATA_PACKET);
                            if(recv_packet.sequence_number < current_frame_index){
                                pthread_mutex_lock(&current_frame_index_mutex);
                                current_frame_index = recv_packet.sequence_number;
                                pthread_mutex_unlock(&current_frame_index_mutex);
                                size_t i = recv_packet.sequence_number;
                                while (frame_list[i].status == ACKNOWLEDGED)
                                {
                                    pthread_mutex_lock(&frame_list_mutex);
                                    frame_list[i].status = NOT_ACKNOWLEDGED;
                                    pthread_mutex_unlock(&frame_list_mutex);
                                    i++;
                                }
                            }
                            thread_status = READ_WRITE_FRAME_BUFFER;
                        } else {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // Timeout occurred
                                printTimeOutError(thread_port, thread_data_package_index);
                            } else {
                                perror("Error receiving data");
                            }
                            thread_status = WAITING_FOR_DATA;
                        }
                    } else {
                        printTimeOutError(thread_port, thread_data_package_index);
                        thread_status = WAITING_FOR_DATA;
                    }
                } else {
                    is_running = 0;
                }
                break;

            case READ_WRITE_FRAME_BUFFER:
                pthread_mutex_lock(&frame_list_mutex);
                recv_packet.frame.status = ACKNOWLEDGED;
                frame_list[recv_packet.sequence_number] = recv_packet.frame;
                pthread_mutex_unlock(&frame_list_mutex);
                thread_status = SENDING_DATA;
                break;

            case SENDING_DATA:
                ack_packet.sequence_number = thread_data_package_index;
                ack_packet.frame.status = ACKNOWLEDGED;
                printDataPacket(current_frame_index, window_end_index, frame_list_last_index, thread_port, ack_packet, SEND_DATA_PACKET);
                send_ack_success_chance = generateRandomNumber();
                if(send_ack_success_chance > CHANCE_FOR_ERROR_IN_SEND_PERCENT){
                    check(
                        (sendto(thread_socket, &ack_packet, sizeof(data_packet_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
                        "Download thread failed to send ack packet.\n"
                    );
                } else {
                    printSendError(thread_port,thread_data_package_index);
                }
                thread_status = NEXT_INDEX;
                break;

            case NEXT_INDEX:
                //Only the thread for the first element in the window can move the window
                if(thread_num == 0){
                    pthread_mutex_lock(&current_frame_index_mutex);
                    current_frame_index++;
                    pthread_mutex_unlock(&current_frame_index_mutex);
                }
                thread_status = WAITING_FOR_DATA;
                break;

            default:
                cout << "Download Thread: " << thread_port << "Unknown Thread status" << thread_status;
                break;
        }
    }
    return 0;
}

void downloadFile(string file_name){
    operation_packet_t operation_packet, ack_operation_packet;
    is_running = 1;
    current_frame_index = 0;
    window_end_index = 0;


    //Starts download from server
    int client_socket;
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);

    operation_packet.ftp_mode = DOWNLOAD;
    strcpy(operation_packet.file_name,file_name.c_str());

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

    //Gets file size.
    size_t file_size = ack_operation_packet.file_size_in_chunks;
    frame_list_last_index = file_size;

    //Prepares file buffer to receive incoming data.
    frame_list = (frame_t *)malloc(file_size * sizeof(frame_t));

    //Creates client threads to handle data server data packets
    for (size_t i = 0; i < WINDOW_SIZE; i++)
    {
        if (pthread_create(&sliding_window[i], NULL, downloadFileThread, (void*)&i) != 0) {
            perror("Error creating download thread.\n");
            return;
        }
        usleep(DELAY_BETWEEN_THREAD_CREATION);
    }

    // Tells the server that it can start sending the data packets
    check(
        (sendto(client_socket, &operation_packet, sizeof(operation_packet_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
        "Failed to send ack operation datagram.\n"
    );

    //When all the file data is received, write the file
    while (is_running){}
    string file_name_string(file_name);
    string file_path = FILE_PATH + file_name_string;
    writeFileFromFrames(file_path.c_str(), frame_list_last_index);
    free(frame_list);
}

#endif /* _SLIDING_WINDOWS_H_ */