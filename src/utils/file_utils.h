#ifndef _FILE_UTILS_H_
#define _FILE_UTILS_H_

#include <iostream>
#include "../types.h"

size_t calculateNumChunks(const char* file_path, size_t chunk_size) {
    FILE* file = fopen(file_path, "rb");

    if (!file) {
        perror("Error opening file");
        return 0; // Return 0 indicating an error
    }

    // Get the size of the file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    fclose(file);

    // Calculate the number of chunks
    size_t num_chunks = (file_size / chunk_size) + ((file_size % chunk_size != 0) ? 1 : 0);

    return num_chunks;
}

#endif /* _FILE_UTILS_H_ */