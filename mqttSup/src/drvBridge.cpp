#include "drvBridge.h"
#include "sys/socket.h"
#include <cerrno>
#include <stdexcept>

TcpServer::TcpServer() {
  onMessage = [](std::string& msg) {
    printf("%s", msg.c_str());
  };
}

TcpServer::~TcpServer() {
  close(socket_fd);
  free(server);
}

void TcpServer::setup() {
  if (running) {
    return;
  }
  int result;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  result = getaddrinfo("127.0.0.1", "1235", &hints, &server);
  if (result == -1) {
    throw std::runtime_error("failed to get addr info: " + std::to_string(errno));
  }
  socket_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
  if (socket_fd == -1) {
    throw std::runtime_error("Failed to create socket");
  }

  // enable socket reuse for re-binding
  int yes = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

  // bind port
  result = bind(socket_fd, server->ai_addr, server->ai_addrlen);
  if (result == -1) {
    close(socket_fd);
    throw std::runtime_error("Failed to bind to socket: " + std::to_string(errno));
  }

  // start accepting connections. expect only 1 connection
  result = listen(socket_fd, 1);
  if (result == -1) {
    close(socket_fd);
    throw std::runtime_error("Failed to listen to socket: " + std::to_string(errno));
  }
}

void TcpServer::disconnect() {
  if (running) {
    running = false;
    close(socket_fd);
  }
}

void TcpServer::start(std::function<void (std::string& msg)> onMessage) {
  if (!running) {
    this->onMessage = onMessage;
    th = std::thread([this]()->void {this->loop();});
    running = true;
  }
}

size_t TcpServer::write(std::string& msg) {
  if (!running) return 0;
  if (msg.size() < 2) return 0;
  size_t cursor = 0;
  // remove all carriage return to prevent interfering with streamdevice parsing
  while(true) {
    cursor = msg.find('\r', cursor);
    if (cursor == std::string::npos) break;
    msg.replace(cursor, 1, "");
  }
  if (msg.substr(msg.size() - 2) != "\r\n") {
    msg.append("\r\n");
  }
  printf("MQTT->TCP: %s\n", msg.c_str());
  return send(client_fd, msg.c_str(), msg.size(), 0);
}

void TcpServer::loop() {
  printf("Loop started\n");
  struct sockaddr_storage client;
  socklen_t addrlen = sizeof client;
  std::string message;
  char buffer[1024];
  ssize_t count;

  while (running) {
    client_fd = accept(socket_fd, (struct sockaddr*)&client, &addrlen);
    if (client_fd == -1 ) {
      running = false;
      break;
    }
    while (true) {
      bzero(buffer, 1024);
      count = recv(client_fd, buffer, 1023, 0);
      if (count == -1 && (errno != EAGAIN || errno != EWOULDBLOCK)) {
        running = false;
        break;
      }
      if (count == 0) {
        running = false;
        break;
      }
      message.append(buffer);
      printf("TCP->MQTT: %s\n", message.c_str());
      if (message.size() >= 2 && message.substr(message.size() - 2) == "\r\n") {
        onMessage(message);
        message.clear();
      }
      continue;
    }
  }
  disconnect();
}
