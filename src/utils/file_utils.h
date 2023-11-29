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

void loadFramesFromFile(const char *filename, frame_t *frame_list, size_t number_of_chunks) {
    FILE *file = fopen(filename, "rb"); // Open the file in binary mode for reading
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Dynamically allocate memory for the frame list
    frame_list = (frame_t *)malloc(number_of_chunks * sizeof(frame_t));
    if (frame_list == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return;
    }

    // Read frames from the file into the dynamically allocated frame list
    for (size_t i = 0; i < number_of_chunks; ++i) {
        frame_list[i].status = i; // Set a sample status value

        // Read data from the file into the data property
        size_t bytesRead = fread(frame_list[i].data, sizeof(char), CHUNK_SIZE, file);

        if (bytesRead < CHUNK_SIZE) {
            // Fill the remaining bytes with zeros:
            for (size_t j = bytesRead; j < CHUNK_SIZE; ++j) {
                frame_list[i].data[j] = 0;
            }
        }
    }

    fclose(file); // Close the file after reading
}

#endif /* _FILE_UTILS_H_ */