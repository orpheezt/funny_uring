#include <cstdio>
#include <print>

#include "server.hpp"

using namespace std;

int main(int argc, char **argv) {
  println(stderr, R"(
  ┌─┐┌─┐┬ ┬┌─┐    ┬ ┬┬─┐┬┌┐┌┌─┐
  ├┤ │  ├─┤│ │    │ │├┬┘│││││ ┬
  └─┘└─┘┴ ┴└─┘────└─┘┴└─┴┘└┘└─┘
  )");

  Server S{/*0.0.0.0*/ 0, 9877};

  S.startup();

  S.run();

  S.shutdown();

  return 0;
}