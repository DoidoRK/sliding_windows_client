#ifndef _TIME_UTILS_H_
#define _TIME_UTILS_H_
#include <iostream>
#include <time.h>

using namespace std;

void printCurrentTime(){
    // Get the current time
    time_t send_time;
    time(&send_time);

    // Convert the time to a tm struct for formatting
    struct tm* send_time_tm = localtime(&send_time);

    // Format and print the current time
    char send_time_buffer[80];
    strftime(send_time_buffer, sizeof(send_time_buffer), "%Y-%m-%d %H:%M:%S", send_time_tm);
    cout << "Current time: " << send_time_buffer << endl;
}

int generateRandomNumber() {
    // Seed the random number generator with the current time
    srand(static_cast<unsigned int>(time(nullptr)));

    // Generate a random number between 0 and 100
    return rand() % 101;
}

#endif /* _TIME_UTILS_H_ */