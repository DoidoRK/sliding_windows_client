#ifndef _TYPES_H_
#define _TYPES_H_

#include <iostream>
#include "config.h"

using namespace std;

enum FTP_MODES {
    DOWNLOAD,
    UPLOAD,
};

enum DATA_PACKET_TYPE {
    RECV_DATA_PACKET,
    SEND_DATA_PACKET,
};

enum FRAME_STATUS {
    NOT_ACKNOWLEDGED,
    ACKNOWLEDGED,
};
typedef struct frame_struct {
    int status;
    char data[CHUNK_SIZE];
} frame_t;

typedef struct operation_packet_struct {
    int ftp_mode;
    char file_name[FILE_NAME_SIZE];
    size_t number_of_chunks_in_file;
} operation_packet_t;

typedef struct data_packet_struct {
    int sequence_number;
    frame_t frame;
} data_packet_t;


#endif /* _TYPES_H_ */