// created by lccc 12/16/2021, no copyright

#include "libdns/client.h"

#include <cassert>
#include <csignal>
#include <iostream>

bool stop;
void quit(int signum) { stop = true; printf("received the signal: %d\n", signum); }

int main(int argc, char **args) {
  libdns::Client client;

  std::vector<std::pair<std::string, std::string>> params = {  // for test
      { "google.com", "AAAA" },
      { "google.com", "A" },
      { "wikipedia.org", "TXT" },
      { "wikipedia.org", "AAAA" },
      { "en.wikipedia.org", "A" },
  };

  if (argc > 1) {
    params.clear();
    params.emplace_back( args[1], "AAAA" );
  }
  if (argc > 2) {
    params[0].second = args[2];
  }

  std::uint32_t done_check = 0;  // for test

  for (const auto& param : params) {
    // query & get callback
    client.query(param.first, libdns::RRS.at(param.second), [params, param, &done_check](std::vector<std::string> data) {
      assert(!data.empty());
      if (param.second == "AAAA") {
        assert(data[0].find(':') != std::string::npos);
      } else if (param.second == "A") {
        assert(data[0].find('.') != std::string::npos);
      }

      for (const auto& row : data) {
        std::cout << param.first << ", " << param.second << " -> " << row << '\n';
      }

      if (++done_check >= params.size()) {
        stop = true;
      }
    });
  }

  stop = false;

  signal(SIGINT, quit); // CTRL + C

  while (!stop) {  // event loop
    client.receive();
  }

  return 0;
}
