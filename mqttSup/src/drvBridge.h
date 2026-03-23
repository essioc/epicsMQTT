#ifndef DRVBRIDGE_H
#define DRVBRIDGE_H
#include <functional>
#include <thread>
#include <unistd.h>
#include <iocsh.h>
#include <sys/un.h>
#include <atomic>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

class TcpServer {
public:
  TcpServer();
  ~TcpServer();
  void disconnect();
  size_t write(std::string& msg);
  void setup();
  void start(std::function<void (std::string& msg)> onMessage);
  void stop();

private:
  std::function<void (std::string& msg)> onMessage;
  void loop();
  int socket_fd;
  int client_fd;
  std::atomic<bool> running = false;
  void generate_address();
  std::thread th;
  struct addrinfo hints, *server;
};

#endif /* DRVBRIDGE_H */
