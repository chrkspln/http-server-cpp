#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <thread>

//#include <zlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

sockaddr_in setting_server_addr();
int server_setup(const int &server_fd, sockaddr_in &server_addr);
void cutting_connection();
int create_socket();
void default_success_response(const int &client_fd);
void default_fail_response(const int &client_fd);
void get_echo_response(const int &client_fd, const std::string &client_msg);
std::string encoding_request(const std::string &client_msg, int start_encoding_index);
void get_user_ag_response(const int &client_fd, const std::string &client_msg);
void get_files_response(const int &client_fd, const std::string &client_msg);
void post_files_response(const int &client_fd, const std::string &client_msg);
void processing_user_request(const int &client_fd, const std::string &client_msg);
void handle_client(const int &client_fd);
//std::string gzip_compress(const std::string &data);

constexpr int buffer = 1024;
constexpr int connection_backlog = 5;
inline int SETUP_SUCCESS{};
inline int SETUP_FAILURE{};
std::atomic<bool> connection_ended{false};
std::vector<std::jthread> threads{};
std::string dir{};
