# UDP Sliding Window Protocol Client

## Overview

This UDP Sliding Window Protocol Client enables reliable file transfer between a client and server using a sliding window approach. It supports both uploading and downloading files.

## Usage

### File Structure

Before running the client, ensure that the files you want to upload are placed in the `files` directory inside the `src` folder.

### Compilation

Compile the client using:
```
make
```

### Running the Client

To run the client, use:

- To upload files to server:
```
make run FTP_MODE=UPLOAD FILE_NAME=myfile.txt
```
- To download files from server:
```
make run FTP_MODE=DOWNLOAD FILE_NAME=myfile.txt
```

## Important Notes

- Adjust `SERVER_IP` and `SERVER_PORT` constants in the source code according to your server's configuration.
- Before uploading files, be sure to place them in ```./src/files``` directory.
- Downloaded files are placed in ```./src/files``` directory.

## Dependencies

- C++ compiler (g++).
- `make` utility.

## Contributing

Feel free to contribute by opening issues, suggesting improvements, or submitting pull requests.

## License

This project is licensed under the [MIT License](LICENSE).
