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

using namespace std;

pthread_t window_element_thread[WINDOW_SIZE];

void *fileUploadThread(void *args){

}

void *fileDownloadThread(void *args){

}

//Creates sliding windows threads to deal with file transfer.
// for (uint8_t i = 0; i < WINDOW_SIZE; i++)
// {
//     pthread_create(&window_element_thread[i],NULL, fileUploadThread, NULL);
// }

//Creates sliding windows threads to deal with file transfer.
// for (uint8_t i = 0; i < WINDOW_SIZE; i++)
// {
//     pthread_create(&window_element_thread[i],NULL, fileDownloadThread, NULL);
// }

void uploadFile(const char *file_name){
    cout << "Fazendo Upload de: " << file_name << endl;
}

void downloadFile(const char *file_name){
    cout << "Fazendo Download de: " << file_name << endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <upload/download> <file_path>" << endl;
        return 1;
    } else {
        int client_socket;
        struct sockaddr_in server_addr;
        socklen_t server_addrLen = sizeof(server_addr);
        string operation = argv[1];
        string file_path = argv[2];
        operation_datagram_t operation_packet, ack_operation_packet;

        struct timeval timeout;      
        timeout.tv_sec = SOCKET_TIMEOUT_IN_SECONDS;
        timeout.tv_usec = SOCKET_TIMEOUT_IN_MICROSSECONDS;
        
        
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

        check(
            (setsockopt (client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof(timeout))),
            "Failed to set socket receive timeout"
        );

        strcpy(operation_packet.file_path, file_path.c_str());
        strcpy(operation_packet.operation, operation.c_str());

        check(
            (sendto(client_socket, &operation_packet, sizeof(operation_datagram_t), 0, (struct sockaddr*)&server_addr, sizeof(server_addr))),
            "Failed to send operation datagram.\n"
        );
        
        // Get the current time
        time_t send_time;
        time(&send_time);

        // Convert the time to a tm struct for formatting
        struct tm* send_time_tm = localtime(&send_time);

        // Format and print the current time
        char send_time_buffer[80];
        strftime(send_time_buffer, sizeof(send_time_buffer), "%Y-%m-%d %H:%M:%S", send_time_tm);
        printf("Send time: %s\n", send_time_buffer);

        // check(
        //     (recvfrom(client_socket, &ack_operation_packet, sizeof(operation_datagram_t), 0, (struct sockaddr*)&server_addr, &server_addrLen)),
        //     "Failed to receive ack operation datagram.\n"
        // );
        // printf("Mensagem de ack recebida.\n");
        recvfrom(client_socket, &ack_operation_packet, sizeof(operation_datagram_t), 0, (struct sockaddr*)&server_addr, &server_addrLen);
        // Get the current time
        time_t current_time;
        time(&current_time);

        // Convert the time to a tm struct for formatting
        struct tm* current_time_tm = localtime(&current_time);

        // Format and print the current time
        char current_time_buffer[80];
        strftime(current_time_buffer, sizeof(current_time_buffer), "%Y-%m-%d %H:%M:%S", current_time_tm);
        printf("Current time: %s\n", current_time_buffer);

        if (operation == "upload") {
            uploadFile(file_path.c_str());
        } else if (operation == "download") {
            downloadFile(file_path.c_str());
        } else {
            cout << "Invalid operation. Please use 'upload' or 'download'." << endl;
            return 1;
        }

        close(client_socket);

        return 0;
    }
}