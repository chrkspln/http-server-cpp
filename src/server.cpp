#include "server.h"

sockaddr_in setting_server_addr() {
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(4221);
	return server_addr;
}

int server_setup(const int &server_fd, sockaddr_in &server_addr) {
	if (bind(server_fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) != 0) {
		std::cerr << "Failed to bind to port 4221\n";
		return SETUP_FAILURE;
	}

	if (listen(server_fd, connection_backlog) != 0) {
		std::cerr << "listen failed\n";
		return SETUP_FAILURE;
	}
	return SETUP_SUCCESS;
}

void cutting_connection() {
	connection_ended.store(true, std::memory_order::release);
	connection_ended.notify_all();
}

int create_socket() {
	const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		std::cerr << "Failed to create server socket\n";
		return SETUP_FAILURE;
	}
	return server_fd;
}

void default_success_response(const int &client_fd) {
	const std::string success_msg = "HTTP/1.1 200 OK\r\n\r\n";
	send(client_fd, success_msg.data(), success_msg.length(), 0);
	cutting_connection();
}

void default_fail_response(const int &client_fd) {
	const std::string fail_msg = "HTTP/1.1 404 Not Found\r\n\r\n";
	send(client_fd, fail_msg.data(), fail_msg.length(), 0);
	cutting_connection();
}

void get_echo_response(const int &client_fd, const std::string &client_msg) {
	std::string get = "GET /echo/";
	const int stop_index = client_msg.find("HTTP");

	const std::string echo_msg = client_msg.substr(get.length(), stop_index - get.length() - 1);

	if (const auto &index = client_msg.find("Accept-Encoding: "); index != std::string::npos
		and client_msg.find("invalid-encoding") == std::string::npos) {
		const std::string encode = encoding_request(client_msg, index);
		if (encode.find("gzip") != std::string::npos) {

			// std::string compressed_msg = gzip_compress(echo_msg);
			const auto compressed_msg = &echo_msg;
			const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
				+ std::to_string(compressed_msg->length()) + "\r\nContent-Encoding: gzip\r\n\r\n" + *compressed_msg;
			send(client_fd, response.data(), response.length(), 0);
			cutting_connection();
			return;
		}
	}

	const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
		+ std::to_string(echo_msg.length()) + "\r\n\r\n" + echo_msg;
	send(client_fd, response.data(), response.length(), 0);
	cutting_connection();
}

std::string encoding_request(const std::string &client_msg, int start_encoding_index) {
	const std::string encoding = "Accept-Encoding: ";
	int stop_index = client_msg.find("\r\n\r\n", start_encoding_index);
	start_encoding_index += encoding.length();
	stop_index -= encoding.length();
	const std::string encoding_type = client_msg.substr(start_encoding_index, stop_index);
	return encoding_type;
}

void get_user_ag_response(const int &client_fd, const std::string &client_msg) {
	const std::string user_ag = "User-Agent: ";
	const int start_i = client_msg.find(user_ag);
	int end_i = client_msg.find("\r\n\r\n", start_i);
	const std::string read_header = client_msg.substr(start_i + user_ag.length(), end_i);

	/*
	 * dealing with trailing '\x00' chars:
	 * client_msg.resize(bytes_received) causes errors for some reason
	 */
	end_i = read_header.find_first_of("\r\n\r\n");
	const std::string out_header = read_header.substr(0, end_i);

	const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " +
		std::to_string(out_header.length()) + "\r\n\r\n" + out_header;
	send(client_fd, response.data(), response.length(), 0);
	cutting_connection();
}

void get_files_response(const int &client_fd, const std::string &client_msg) {
	std::string get_files = "GET /files/";
	const int stop_index = client_msg.find("HTTP");
	const std::string echo_file = client_msg.substr(get_files.length() - 1, stop_index - get_files.length());
	std::ifstream ifs(dir + echo_file);
	if (ifs.good()) {
		std::stringstream content{};
		content << ifs.rdbuf();
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "
			+ std::to_string(content.str().length()) + "\r\n\r\n" + content.str() + "\r\n";
		send(client_fd, response.data(), response.length(), 0);
	} else {
		default_fail_response(client_fd);
	}
}

void post_files_response(const int &client_fd, const std::string &client_msg) {
	std::string post_files = "POST /files/";
	int stop_index = client_msg.find("HTTP");
	const std::string echo_file = client_msg.substr(post_files.length(), stop_index - post_files.length() - 1);

	std::string content_len_msg = "Content-Length: ";
	int start_index = client_msg.find(content_len_msg);
	stop_index = client_msg.find("\r\n\r\n", start_index);
	const std::string echo_file_contents_len = client_msg.substr(start_index + content_len_msg.length() - 1, stop_index - content_len_msg.length());

	// dealing with trailing '\x00' chars (again)
	start_index = client_msg.find("\r\n\r\n");
	const std::string echo_file_contents = client_msg.substr(start_index + 4, std::stoi(echo_file_contents_len));

	std::ofstream outfile(dir + echo_file);
	outfile << echo_file_contents;
	outfile.close();

	std::string response = "HTTP/1.1 201 Created\r\n\r\n";
	send(client_fd, response.data(), response.length(), 0);
}

void processing_user_request(const int &client_fd, const std::string &client_msg) {
	if (client_msg.starts_with("GET / HTTP/1.1\r\n")) {
		default_success_response(client_fd);
	} else if (client_msg.starts_with("GET /echo/")) {
		get_echo_response(client_fd, client_msg);
	} else if (client_msg.starts_with("GET /user-agent")) {
		get_user_ag_response(client_fd, client_msg);
	} else if (client_msg.starts_with("GET /files/")) {
		get_files_response(client_fd, client_msg);
	} else if (client_msg.starts_with("POST /files/")) {
		post_files_response(client_fd, client_msg);
	} else {
		default_fail_response(client_fd);
	}
}

void handle_client(const int &client_fd) {
	std::string client_msg(buffer, '\0');
	auto const bytes_received = recv(client_fd, client_msg.data(), buffer, 0);

	if (bytes_received < 0) {
		std::cerr << "Failed to receive message from client\n";
		close(client_fd);
		return;
	}

	processing_user_request(client_fd, client_msg);
	close(client_fd);
}

//std::string gzip_compress(const std::string &data) {
//	z_stream zs;
//	memset(&zs, 0, sizeof(zs));
//	if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
//		throw std::runtime_error("deflateInit2 failed while compressing.");
//	}
//	zs.next_in = (Bytef *)data.data();
//	zs.avail_in = data.size();
//	int ret;
//	char outbuffer[32768]{};
//	std::string outstring{};
//	do {
//		zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
//		zs.avail_out = sizeof(outbuffer);
//		ret = deflate(&zs, Z_FINISH);
//		if (outstring.size() < zs.total_out) {
//			outstring.append(outbuffer, zs.total_out - outstring.size());
//		}
//	} while (ret == Z_OK);
//	deflateEnd(&zs);
//	if (ret != Z_STREAM_END) {
//		throw std::runtime_error("Exception during zlib compression: (" + std::to_string(ret) + ") " + zs.msg);
//	}
//	return outstring;
//}


int main(int argc, char **argv) {
	if (argc == 3 && strcmp(argv[1], "--directory") == 0) {
		dir = argv[2];
	}

	const int server_fd = create_socket();
	sockaddr_in server_addr = setting_server_addr();
	const int setup = server_setup(server_fd, server_addr);
	if (setup == SETUP_FAILURE) { return 0; }

	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	constexpr int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		std::cerr << "setsockopt failed\n";
		return SETUP_FAILURE;
	}

	while (threads.size() < connection_backlog) {
		sockaddr_in client_addr;
		int client_addr_len = sizeof(client_addr);

		const int client_fd = accept(server_fd, reinterpret_cast<struct sockaddr *>(&client_addr),
									 reinterpret_cast<socklen_t *>(&client_addr_len));

		if (client_fd < 0) {
			std::cerr << "Failed to accept client connection\n";
			std::cerr << "Error: " << strerror(errno) << std::endl;
			continue;
		}

		threads.emplace_back([client_fd]() {
			handle_client(client_fd);
							 });
	}

	close(server_fd);

	return 0;
}