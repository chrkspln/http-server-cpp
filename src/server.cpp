#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

constexpr int BUFFER = 1024;

int main() {
	const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		std::cerr << "Failed to create server socket\n";
		return 1;
	}

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

	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	std::cout << "Waiting for a client to connect...\n";

	const int client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr),
		reinterpret_cast<socklen_t*>(&client_addr_len));

	if (client_fd < 0) {
		std::cerr << "Failed to accept client connection\n";
		return 1;
	}

	std::string client_msg(BUFFER, '\0');
	const std::string success_msg = "HTTP/1.1 200 OK\r\n\r\n";
	const std::string fail_msg = "HTTP/1.1 404 Not Found\r\n\r\n";

	auto const bytes_received = recv(client_fd, client_msg.data(), 512, 0);

	if (client_msg.starts_with("GET / HTTP/1.1\r\n")) {
		send(client_fd, success_msg.data(), success_msg.length(), 0);
	}
	else if (client_msg.substr(2, 7) == "/echo/") {
		const int stop_index = client_msg.find("\r\n");
		const std::string echo_msg = client_msg.substr(9, stop_index - 9);
		const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
			+ std::to_string(echo_msg.length()) + "\r\n\r\n" + echo_msg;
		send(client_fd, response.data(), response.length(), 0);
	}
	else {
		send(client_fd, fail_msg.data(), fail_msg.length(), 0);
	}

	close(server_fd);

	return 0;
}