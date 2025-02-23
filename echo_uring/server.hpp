#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include <cstdint>

#include <liburing.h>

#include <arpa/inet.h>
#include <sys/socket.h>

struct ServerConfig {
  uint32_t maxConn = 2;
  uint32_t bufferSz = 512;
  uint32_t ioRingSlots = 2;
};

enum EventType { ACCEPT, OPEN, RECV, SEND, CLOSE };

struct ClientInfo {
  int S;
  sockaddr_in addr;
  socklen_t len = sizeof(sockaddr_in);
};

struct Request {
  EventType eTy;
  char *buffer;
  ClientInfo client;
};

class Server {
public:
  Server(uint32_t listen, uint16_t port, ServerConfig config = {})
      : listenAddr{listen}, port{port}, config{config} {}

  bool startup() {
    S = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{.sin_family = AF_INET,
                        .sin_port = htons(port),
                        .sin_addr = {.s_addr = htonl(listenAddr)}};

    bind(S, (sockaddr *)&address, sizeof(sockaddr_in));

    listen(S, config.maxConn);

    io_uring_queue_init(config.ioRingSlots, &ring, 0);

    return true;
  }

  bool shutdown() {
    io_uring_queue_exit(&ring);
    close(S);

    return true;
  }

  void run() {
    submitAccept();
    io_uring_cqe *cqe;
    while (true) {
      io_uring_wait_cqe(&ring, &cqe);
      auto *request = (Request *)cqe->user_data;
      switch (request->eTy) {
      case ACCEPT: {
        submitAccept();
        request->client.S = (int)cqe->res;
        submitRecv(request);
        break;
      }
      case OPEN: {
        break;
      }
      case RECV: {
        auto bytesRead = (int)cqe->res;
        if (bytesRead > 0) {
          submitSend(request);
        } else if (bytesRead == 0) {
          submitClose(request);
        }
        break;
      }
      case SEND: {
        auto bytesSent = (int)cqe->res;
        if (bytesSent > 0) {
          submitRecv(request);
        }
        break;
      }
      case CLOSE: {
        delete request;
        break;
      }
      }

      io_uring_cqe_seen(&ring, cqe);
    }
  }

private:
  void submitAccept() {
    auto *sqe = io_uring_get_sqe(&ring);

    auto *request = new Request;
    request->buffer = new char[config.bufferSz];
    request->eTy = EventType::ACCEPT;

    io_uring_prep_accept(sqe, S, (sockaddr *)&request->client.addr,
                         &request->client.len, 0);
    io_uring_sqe_set_data(sqe, request);
    io_uring_submit(&ring);
  }

  void submitRecv(Request *request) {
    auto *sqe = io_uring_get_sqe(&ring);

    request->eTy = EventType::RECV;

    io_uring_prep_recv(sqe, request->client.S, request->buffer, config.bufferSz,
                       0);
    io_uring_sqe_set_data(sqe, request);
    io_uring_submit(&ring);
  }
  void submitSend(Request *request) {
    auto *sqe = io_uring_get_sqe(&ring);

    request->eTy = EventType::SEND;

    io_uring_prep_send(sqe, request->client.S, request->buffer, config.bufferSz,
                       0);
    io_uring_sqe_set_data(sqe, request);
    io_uring_submit(&ring);
  }

  void submitClose(Request *request) {
    auto *sqe = io_uring_get_sqe(&ring);

    request->eTy = EventType::CLOSE;

    io_uring_prep_close(sqe, request->client.S);
    io_uring_sqe_set_data(sqe, request);
    io_uring_submit(&ring);
  }

  const uint32_t listenAddr;
  const uint16_t port;
  const ServerConfig config;

  int S;
  io_uring ring;
};

#endif