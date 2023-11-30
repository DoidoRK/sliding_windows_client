#include <iostream>
#include "./sliding_windows.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <upload/download> <file_path>" << endl;
        return 1;
    } else {
        int operation = atoi(argv[1]);
        string file_name = argv[2];
        switch (operation)
        {
            case DOWNLOAD:
                downloadFile(file_name.c_str());
                break;

            case UPLOAD:
                uploadFile(file_name.c_str());
                break;
                
            default:
                cout << "Invalid operation. Please use 'upload' or 'download'." << endl;
                return 1;
                break;
        }
        return 0;
    }
}