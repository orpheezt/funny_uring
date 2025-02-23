/*
clang++ -std=c++23 -Wall -O2 \
-I${HOME}/apps/liburing/include \
-L${HOME}/apps/liburing/lib -luring \
echo.cpp -o echo
*/

#include <cstdio>
#include <print>

#include <liburing.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#ifndef LISTEN_CONN
#define LISTEN_CONN 2
#endif

#ifndef REQ_BUFFER_SZ
#define REQ_BUFFER_SZ 512
#endif

#ifndef QUEUE_DEPTH
#define QUEUE_DEPTH 4
#endif

using namespace std;

struct EchoState {
  io_uring ring;
  int serverSocket;
};

enum EventType { ACCEPT, OPEN, RECV, SEND, CLOSE };

struct Request {
  EventType eTy;
  int clientSocket;
  char buffer[REQ_BUFFER_SZ];
  sockaddr_in addr;
  socklen_t len = sizeof(sockaddr_in);
};

void submitAccept(EchoState &st) {
  auto *sqe = io_uring_get_sqe(&st.ring);
  auto *request = new Request;
  io_uring_prep_accept(sqe, st.serverSocket, (sockaddr *)&request->addr,
                       &request->len, 0);
  request->eTy = EventType::ACCEPT;
  io_uring_sqe_set_data(sqe, request);
  io_uring_submit(&st.ring);
}

void submitRecv(EchoState &st, Request *request) {
  auto *sqe = io_uring_get_sqe(&st.ring);
  io_uring_prep_recv(sqe, request->clientSocket, request->buffer, REQ_BUFFER_SZ,
                     0);
  request->eTy = EventType::RECV;
  io_uring_sqe_set_data(sqe, request);
  io_uring_submit(&st.ring);
}

void submitSend(EchoState &st, Request *request) {
  auto *sqe = io_uring_get_sqe(&st.ring);
  io_uring_prep_send(sqe, request->clientSocket, request->buffer, REQ_BUFFER_SZ,
                     0);
  request->eTy = EventType::SEND;
  io_uring_sqe_set_data(sqe, request);
  io_uring_submit(&st.ring);
}

void submitClose(EchoState &st, Request *request) {
  auto *sqe = io_uring_get_sqe(&st.ring);
  io_uring_prep_close(sqe, request->clientSocket);
  request->eTy = EventType::CLOSE;
  io_uring_sqe_set_data(sqe, request);
  io_uring_submit(&st.ring);
}

void serverLoop(EchoState &st) {
  submitAccept(st);
  io_uring_cqe *cqe;
  while (true) {
    io_uring_wait_cqe(&st.ring, &cqe);
    auto *request = (Request *)cqe->user_data;
    switch (request->eTy) {
    case ACCEPT: {
      submitAccept(st);
      request->clientSocket = (int)cqe->res;
      submitRecv(st, request);
      break;
    }
    case OPEN: {
      break;
    }
    case RECV: {
      auto bytesRead = (int)cqe->res;
      if (bytesRead > 0) {
        submitSend(st, request);
      } else if (bytesRead == 0) {
        submitClose(st, request);
      }
      break;
    }
    case SEND: {
      auto bytesSent = (int)cqe->res;
      if (bytesSent > 0) {
        submitRecv(st, request);
      }
      break;
    }
    case CLOSE: {
      delete request;
      break;
    }
    }

    io_uring_cqe_seen(&st.ring, cqe);
  }
}

int main(int argc, char **argv) {
  println(stderr, R"(
  ┌─┐┌─┐┬ ┬┌─┐
  ├┤ │  ├─┤│ │
  └─┘└─┘┴ ┴└─┘
  )");

  println(stderr, R"(CONFIGURATION:
    LISTEN_CONN: {}
    REQ_BUFFER_SZ: {}
    QUEUE_DEPTH: {}
    )",
          LISTEN_CONN, REQ_BUFFER_SZ, QUEUE_DEPTH);

  EchoState st;

  st.serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in address{.sin_family = AF_INET,
                      .sin_port = htons(9877),
                      .sin_addr = {.s_addr = htonl(INADDR_ANY)}};

  io_uring_queue_init(QUEUE_DEPTH, &st.ring, 0);

  bind(st.serverSocket, (sockaddr *)&address, sizeof(sockaddr_in));

  listen(st.serverSocket, LISTEN_CONN);

  serverLoop(st);

  io_uring_queue_exit(&st.ring);

  close(st.serverSocket);

  return 0;
}