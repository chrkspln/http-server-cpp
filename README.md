# Simple HTTP Server

This project is a simple HTTP server written in C++. It handles basic HTTP requests including GET and POST methods and serves files from a specified directory. The server responds with appropriate HTTP status codes and can also echo messages back to the client.

## Features

- Handles basic HTTP GET and POST requests.
- Serves files from a specified directory.
- Echoes messages back to the client.
- Responds with appropriate HTTP status codes (200 OK, 404 Not Found, 201 Created).
- Supports `User-Agent` inspection.
- Multi-threaded to handle multiple client connections simultaneously.

## Requirements

- A C++ compiler that supports C++11 or later.
- POSIX-compliant operating system (e.g., Linux, macOS).

## Usage

1. **Compile the server:**

    ```sh
    g++ -std=c++11 -pthread -o server main.cpp
    ```

2. **Run the server:**

    ```sh
    ./server --directory <path_to_directory>
    ```

    Replace `<path_to_directory>` with the path to the directory you want to serve files from.

## Code Overview

### Functions

- `sockaddr_in setting_server_addr()`
  - Configures and returns the server address structure.

- `int server_setup(const int &server_fd, sockaddr_in &server_addr)`
  - Binds the server to a port and starts listening for incoming connections.

- `void cutting_connection()`
  - Signals the end of a connection.

- `int create_socket()`
  - Creates and returns a server socket.

- `void default_success_response(const int &client_fd)`
  - Sends a default success (200 OK) response to the client.

- `void default_fail_response(const int &client_fd)`
  - Sends a default failure (404 Not Found) response to the client.

- `void get_echo_response(const int &client_fd, const std::string &client_msg)`
  - Handles GET requests to `/echo/` by echoing back the message.

- `std::string encoding_request(const std::string &client_msg, int start_encoding_index)`
  - Parses and returns the encoding type from the client's request.

- `void get_user_ag_response(const int &client_fd, const std::string &client_msg)`
  - Handles GET requests to `/user-agent` by returning the client's User-Agent.

- `void get_files_response(const int &client_fd, const std::string &client_msg)`
  - Handles GET requests to `/files/` by serving the requested file.

- `void post_files_response(const int &client_fd, const std::string &client_msg)`
  - Handles POST requests to `/files/` by saving the uploaded file.

- `void processing_user_request(const int &client_fd, const std::string &client_msg)`
  - Processes the client's request and routes it to the appropriate handler.

- `void handle_client(const int &client_fd)`
  - Receives the client's message and processes the request.

### Main Function

- Sets up the server socket.
- Binds to the port and starts listening.
- Accepts client connections and spawns threads to handle each client.