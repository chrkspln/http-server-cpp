#include <iostream>
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
	const int stop_index = client_msg.find("HTTP");

	// Extract the message to be sent back to the client:
	// 10 is the length of "GET /echo/", 11 is the length of "GET /echo/ plus whitespace at the end of the word"
	const std::string echo_msg = client_msg.substr(10, stop_index - 11);
	const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
		+ std::to_string(echo_msg.length()) + "\r\n\r\n" + echo_msg;
	send(client_fd, response.data(), response.length(), 0);
	connection_ended.store(true, std::memory_order::release);
	connection_ended.notify_all();
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

void processing_user_request(const int& server_fd) {
	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	const int client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr),
								 reinterpret_cast<socklen_t*>(&client_addr_len));

	if (client_fd < 0) {
		std::cerr << "Failed to accept client connection\n";
		std::cerr << "Error: " << strerror(errno) << std::endl;
		exit(1);
	}

	std::string client_msg(buffer, '\0');
	auto const bytes_received = recv(client_fd, client_msg.data(), buffer, 0);

	if (bytes_received < 0) {
		std::cerr << "Failed to receive message from client\n";
		exit(1);
	}

	if (client_msg.starts_with("GET / HTTP/1.1\r\n")) {
		default_success_response(client_fd);
	} else if (client_msg.starts_with("GET /echo/")) {
		get_echo_response(client_fd, client_msg);
	} else if (client_msg.starts_with("GET /user-agent")) {
		get_user_ag_response(client_fd, client_msg);
	} else {
		default_fail_response(client_fd);
	}
}

int main() {
	const int server_fd = create_socket();
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	constexpr int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		std::cerr << "setsockopt failed\n";
		return 1;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(4221);

	if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
		std::cerr << "Failed to bind to port 4221\n";
		return 1;
	}

	if (listen(server_fd, connection_backlog) != 0) {
		std::cerr << "listen failed\n";
		return 1;
	}

	while (threads.size() < connection_backlog) {
		threads.emplace_back([server_fd]() {
			processing_user_request(server_fd);
							 });
	}

	close(server_fd);

	return 0;
}