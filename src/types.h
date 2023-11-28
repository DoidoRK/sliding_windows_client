#ifndef _WORDLE_TYPES_H_
#define _WORDLE_TYPES_H_

#include <iostream>
#include "config.h"

using namespace std;

typedef struct operation_datagram_struct {
    char operation[OPERATION_BUFFER_SIZE];
    char file_path[FILE_PATH_SIZE];
} operation_datagram_t;

#endif /* _WORDLE_TYPES_H_ */