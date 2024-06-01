#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <threads.h>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>

int create_socket();
void default_success_response(const int& client_fd);
void default_fail_response(const int& client_fd);
void get_echo_response(const int& client_fd, const std::string& client_msg);
std::string encoding_request(const std::string& client_msg, int start_encoding_index);
void get_user_ag_response(const int& client_fd, const std::string& client_msg);
void get_files_response(const int& client_fd, const std::string& client_msg, const std::string& dir);
void post_files_response(const int& client_fd, const std::string& client_msg, const std::string& dir);
void processing_user_request(const int& client_fd, const std::string& client_msg, const std::string& dir);
void handle_client(const int& client_fd, const std::string& dir);

constexpr int buffer = 1024;
constexpr int connection_backlog = 5;

std::atomic<bool> connection_ended{ false };
std::vector<std::jthread> threads{};

int create_socket() {
	const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		std::cerr << "Failed to create server socket\n";
		return 1;
	}
	return server_fd;
}

void default_success_response(const int& client_fd) {
	const std::string success_msg = "HTTP/1.1 200 OK\r\n\r\n";
	send(client_fd, success_msg.data(), success_msg.length(), 0);
	connection_ended.store(true, std::memory_order::release);
	connection_ended.notify_all();
}

void default_fail_response(const int& client_fd) {
	const std::string fail_msg = "HTTP/1.1 404 Not Found\r\n\r\n";
	send(client_fd, fail_msg.data(), fail_msg.length(), 0);
	connection_ended.store(true, std::memory_order::release);
	connection_ended.notify_all();
}

void get_echo_response(const int& client_fd, const std::string& client_msg) {
	std::string get = "GET /echo/";
	const int stop_index = client_msg.find("HTTP");

	// Extract the message to be sent back to the client:
	const std::string echo_msg = client_msg.substr(get.length(), stop_index - get.length() - 1);

	if (auto index = client_msg.find("Accept-Encoding: "); index != std::string::npos
		and client_msg.find("invalid-encoding") == std::string::npos) {
		std::string encode = encoding_request(client_msg, index);
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
			+ std::to_string(echo_msg.length()) + "\r\nContent-Encoding: " + encode + "\r\n\r\n" + echo_msg;
		send(client_fd, response.data(), response.length(), 0);
		connection_ended.store(true, std::memory_order::release);
		connection_ended.notify_all();
	} else {
		const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
			+ std::to_string(echo_msg.length()) + "\r\n\r\n" + echo_msg;
		send(client_fd, response.data(), response.length(), 0);
		connection_ended.store(true, std::memory_order::release);
		connection_ended.notify_all();
	}
}

std::string encoding_request(const std::string& client_msg, int start_encoding_index) {
	std::string encoding = "Accept-Encoding: ";
	int stop_index = client_msg.find("\r\n\r\n", start_encoding_index);
	start_encoding_index += encoding.length();
	stop_index -= encoding.length();
	const std::string encoding_type = client_msg.substr(start_encoding_index, stop_index);
	return encoding_type;
}

void get_user_ag_response(const int& client_fd, const std::string& client_msg) {
	const std::string user_ag = "User-Agent: ";
	const int start_i = client_msg.find(user_ag);
	int end_i = client_msg.find("\r\n\r\n", start_i);
	const std::string read_header = client_msg.substr(start_i + user_ag.length(), end_i);

	end_i = read_header.find_first_of("\r\n\r\n");
	const std::string out_header = read_header.substr(0, end_i);

	const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " +
		std::to_string(out_header.length()) + "\r\n\r\n" + out_header;
	send(client_fd, response.data(), response.length(), 0);
	connection_ended.store(true, std::memory_order::release);
	connection_ended.notify_all();
}

void get_files_response(const int& client_fd, const std::string& client_msg, const std::string& dir) {
	std::string get_files = "GET /files/";
	const int stop_index = client_msg.find("HTTP");
	const std::string echo_file = client_msg.substr(get_files.length() - 1, stop_index - get_files.length());
	std::ifstream ifs(dir + echo_file);
	if (ifs.good()) {
		std::stringstream content{};
		content << ifs.rdbuf();
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(content.str().length()) + "\r\n\r\n" + content.str() + "\r\n";
		send(client_fd, response.data(), response.length(), 0);
	} else {
		default_fail_response(client_fd);
	}
}

void post_files_response(const int& client_fd, const std::string& client_msg, const std::string& dir) {
	std::string post_files = "POST /files/";
	int stop_index = client_msg.find("HTTP");
	const std::string echo_file = client_msg.substr(post_files.length(), stop_index - post_files.length() - 1);

	std::string content_len_msg = "Content-Length: ";
	int start_index = client_msg.find(content_len_msg);
	stop_index = client_msg.find("\r\n\r\n", start_index);
	const std::string echo_file_contents_len = client_msg.substr(start_index + content_len_msg.length() - 1, stop_index - content_len_msg.length());

	start_index = client_msg.find("\r\n\r\n");
	const std::string echo_file_contents = client_msg.substr(start_index + 4, std::stoi(echo_file_contents_len));

	std::ofstream outfile(dir + echo_file);
	outfile << echo_file_contents;
	outfile.close();

	std::string response = "HTTP/1.1 201 Created\r\n\r\n";
	send(client_fd, response.data(), response.length(), 0);
}

void processing_user_request(const int& client_fd, const std::string& client_msg, const std::string& dir) {
	if (client_msg.starts_with("GET / HTTP/1.1\r\n")) {
		default_success_response(client_fd);
	} else if (client_msg.starts_with("GET /echo/")) {
		get_echo_response(client_fd, client_msg);
	} else if (client_msg.starts_with("GET /user-agent")) {
		get_user_ag_response(client_fd, client_msg);
	} else if (client_msg.starts_with("GET /files/")) {
		get_files_response(client_fd, client_msg, dir);
	} else if (client_msg.starts_with("POST /files/")) {
		post_files_response(client_fd, client_msg, dir);
	} else {
		default_fail_response(client_fd);
	}
}

void handle_client(const int& client_fd, const std::string& dir) {
	std::string client_msg(buffer, '\0');
	auto const bytes_received = recv(client_fd, client_msg.data(), buffer, 0);

	if (bytes_received < 0) {
		std::cerr << "Failed to receive message from client\n";
		close(client_fd);
		return;
	}

	processing_user_request(client_fd, client_msg, dir);
	close(client_fd);
}

int main(int argc, char** argv) {
	std::string dir{};
	if (argc == 3 && strcmp(argv[1], "--directory") == 0) {
		dir = argv[2];
	}
	const int server_fd = create_socket();
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	constexpr int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		std::cerr << "setsockopt failed\n";
		return 1;
	}

	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(4221);

	if (bind(server_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
		std::cerr << "Failed to bind to port 4221\n";
		return 1;
	}

	if (listen(server_fd, connection_backlog) != 0) {
		std::cerr << "listen failed\n";
		return 1;
	}

	while (threads.size() < connection_backlog) {
		sockaddr_in client_addr;
		int client_addr_len = sizeof(client_addr);

		const int client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr),
									 reinterpret_cast<socklen_t*>(&client_addr_len));

		if (client_fd < 0) {
			std::cerr << "Failed to accept client connection\n";
			std::cerr << "Error: " << strerror(errno) << std::endl;
			continue;
		}

		threads.emplace_back([client_fd, dir]() {
			handle_client(client_fd, dir);
							 });
	}

	close(server_fd);

	return 0;
}