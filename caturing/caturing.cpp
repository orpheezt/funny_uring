/*
clang++ -std=c++23 -Wall -O2 \
-I$<liburing_include> \
-L$<liburing_lib> -luring \
caturing.cpp -o caturing \
-D BLOCK_SZ=2048 -D QUEUE_DEPTH=2
*/

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <print>

#include <liburing.h>

using namespace std;
namespace fs = filesystem;

#ifndef QUEUE_DEPTH
#define QUEUE_DEPTH 1
#endif

#ifndef BLOCK_SZ
#define BLOCK_SZ 1024
#endif

struct FileInfo {
  uint64_t sz;
  iovec *iovecs;
};

bool submitRead(fs::path p, io_uring *ring) {
  auto File = open(p.c_str(), O_RDONLY);
  if (File < 0) {
    println(stderr, "[{}][{}:{}]: {}", "open", fs::absolute(__FILE__).c_str(),
            __LINE__, strerror(errno));
    return false;
  }

  uint64_t sz = fs::file_size(p);
  uint64_t bytesRemaining = sz;

  uint64_t currentBlock = 0;
  uint64_t blocks = sz / BLOCK_SZ;
  if (sz % BLOCK_SZ != 0)
    blocks++;

  auto *FI = new FileInfo;
  FI->iovecs = new iovec[blocks];

  while (bytesRemaining > 0) {
    auto bytesToRead = (bytesRemaining > BLOCK_SZ) ? BLOCK_SZ : bytesRemaining;
    FI->iovecs[currentBlock].iov_len = bytesToRead;

    auto *vBuffer = new char[BLOCK_SZ];
    FI->iovecs[currentBlock].iov_base = vBuffer;
    currentBlock++;
    bytesRemaining -= bytesToRead;
  }
  FI->sz = sz;

  auto *sqe = io_uring_get_sqe(ring);
  io_uring_prep_readv(sqe, File, FI->iovecs, blocks, 0);
  io_uring_sqe_set_data(sqe, FI);
  io_uring_submit(ring);

  return true;
}

bool handleRead(io_uring *ring) {
  io_uring_cqe *cqe;
  if (io_uring_wait_cqe(ring, &cqe) < 0) {
    println(stderr, "[{}][{}:{}]: {}", "io_uring_wait_cqe",
            fs::absolute(__FILE__).c_str(), __LINE__, strerror(errno));
    return false;
  }
  if (cqe->res < 0) {
    println(stderr, "[{}][{}:{}]: {}", "readv", fs::absolute(__FILE__).c_str(),
            __LINE__, strerror(-cqe->res));
    return false;
  }
  auto *FI = (FileInfo *)io_uring_cqe_get_data(cqe);
  auto blocks = FI->sz / BLOCK_SZ;
  if (FI->sz % BLOCK_SZ)
    blocks++;

  for (int i = 0; i < blocks; i++)
    print("{}",
          string_view{(char *)FI->iovecs[i].iov_base, FI->iovecs[i].iov_len});

  io_uring_cqe_seen(ring, cqe);

  return true;
}

int main(int argc, char **argv) {
  println(stderr, R"(
    ██████  █████  ████████ ██    ██ ██████  ██ ███    ██  ██████  
   ██      ██   ██    ██    ██    ██ ██   ██ ██ ████   ██ ██       
   ██      ███████    ██    ██    ██ ██████  ██ ██ ██  ██ ██   ███ 
   ██      ██   ██    ██    ██    ██ ██   ██ ██ ██  ██ ██ ██    ██ 
    ██████ ██   ██    ██     ██████  ██   ██ ██ ██   ████  ██████  
   )");

  if (argc < 2) {
    println(stderr, "Usage: caturing [file name] <[file name] ...>");
    return EXIT_FAILURE;
  }

  println(stderr, R"(CONFIGURATION:
    QUEUE_DEPTH: {}
    BLOCK_SZ: {}
    )",
          QUEUE_DEPTH, BLOCK_SZ);

  io_uring ring;
  io_uring_queue_init(QUEUE_DEPTH, &ring, 0);

  for (uint32_t i = 1; i < argc; i++) {
    if (!submitRead(argv[i], &ring)) {
      println(stderr, "Error submiting read on: {}", argv[i]);
      return 1;
    }
    if (!handleRead(&ring)) {
      println(stderr, "Error handling read on: {}", argv[i]);
      return 1;
    }
  }

  io_uring_queue_exit(&ring);

  return 0;
}