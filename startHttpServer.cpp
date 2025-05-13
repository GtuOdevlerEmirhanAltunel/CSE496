#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

#include "defines.hpp"
#include "utils.hpp"

void startHttpServer() {
  int serverFd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd == -1) {
    perror("[HttpServer] socket creation failed");
    return;
  }

  // Allow address reuse
  int opt = 1;
  if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("[HttpServer] setsockopt SO_REUSEADDR failed");
    close(serverFd);
    return;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(HTTP_PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("[HttpServer] bind failed");
    close(serverFd);
    return;
  }

  if (listen(serverFd, 10) < 0) {  // Increased backlog queue
    perror("[HttpServer] listen failed");
    close(serverFd);
    return;
  }

  std::cout << "[HttpServer] Serving on http://0.0.0.0:" << HTTP_PORT
            << std::endl;

  while (true) {
    sockaddr_in clientAddr{};
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientAddrLen);

    if (clientFd < 0) {
      perror("[HttpServer] accept failed");
      continue;  // Continue to accept other connections
    }

    // For simplicity, handling each client sequentially.
    // For concurrent handling, you'd typically spawn a new thread here:
    // std::thread(handleHttpClient, clientFd).detach();
    // However, for RPi Zero, sequential might be more stable without careful
    // thread management. The /live endpoint itself is blocking for that
    // connection.
    handleHttpClient(
        clientFd);  // This will block for /live until client disconnects
  }
  close(serverFd);  // Should be unreachable in this loop
}
