#include <iostream>
#include "./sliding_windows.h"

using namespace std;

const string UPLOAD_MODE = "UPLOAD";
const string DOWNLOAD_MODE = "DOWNLOAD";

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <UPLOAD/DOWNLOAD> <file_path>" << endl;
        return 1;
    } else {
        string operation = argv[1];
        string file_name = argv[2];
        // Seed the random number generator with the current time to simulate time out occurring
        srand(static_cast<unsigned int>(time(nullptr)));
        if(operation == UPLOAD_MODE){
            uploadFile(file_name.c_str());
        } else if (operation == DOWNLOAD_MODE){
            downloadFile(file_name.c_str());
        } else {
            cout << "Invalid operation." << endl;
            cout << "Example case:" << endl;
            cout << "make run FTP_MODE=UPLOAD FILE_NAME=myfile.txt" << endl;
        }
        return 0;
    }
}