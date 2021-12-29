// created by lccc 12/29/2021, no copyright

#include "libdns/client.h"

#include <iostream>

#include <arpa/inet.h>

int main() {
  auto client = libdns::Client(1);

  std::string host = "upload.wikimedia.org";
  std::string path = "/wikipedia/commons/c/ca/En-us-exerting.ogg";
  client.query(host, LIBDNS_WITH_IPV6 ? 28 : 1, [&client,host,path](std::vector<std::string> dns_data) {
    if (dns_data.empty()) {
      exit(1);
    }
    std::string ip = dns_data[0];
    client.send_https_request(LIBDNS_WITH_IPV6 ? AF_INET6 : AF_INET, ip, host, path, [path](std::vector<std::vector<char>> res) {
      std::vector<char> body = res[1];

      std::cout << "body.size: " << body.size() << std::endl;
      if (body.size() != 26234) {
        exit(2);
      }
    });
  });

  for (int i = 0; i < 99; i++) { client.receive(9); }
}