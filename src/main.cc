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
//Creates sliding windows threads to deal with file transfer.
// for (uint8_t i = 0; i < WINDOW_SIZE; i++)
// {
//     pthread_create(&window_element_thread[i],NULL, connectionThread, NULL);
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
        int clientSocket;
        struct sockaddr_in serverAddr;
        string operation = argv[1];
        string file_path = argv[2];
        operation_datagram_t operation_packet;

        check(
            (clientSocket = socket(AF_INET, SOCK_DGRAM, 0)),
            "Failed to create client socket"
        );

        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(SERVER_PORT);
        check(
            (inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr)),
            "Failed to set server address.\n"
        );

        strcpy(operation_packet.file_path, file_path.c_str());
        strcpy(operation_packet.operation, operation.c_str());

        check(
            (sendto(clientSocket, &operation_packet, sizeof(operation_datagram_t), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr))),
            "Failed to send operation datagram.\n"
        );

        if (operation == "upload") {
            uploadFile(file_path.c_str());
        } else if (operation == "download") {
            downloadFile(file_path.c_str());
        } else {
            cout << "Invalid operation. Please use 'upload' or 'download'." << endl;
            return 1;
        }

        close(clientSocket);

        return 0;
    }
}