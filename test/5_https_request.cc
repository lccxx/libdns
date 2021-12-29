// created by lccc 12/19/2021, no copyright

#include "libdns/client.h"
#include "rapidjson/document.h"

#include <cassert>
#include <iostream>
#include <random>

#include <arpa/inet.h>

int main() {
  auto client = libdns::Client(1);

  std::mt19937 rand_engine = std::mt19937((std::random_device())());

  for (int i = 0; i < 9; i ++) {
    client.query("google.com", LIBDNS_WITH_IPV6 ? 28 : 1, [&client, &rand_engine, i](std::vector<std::string> dns_data) {
      assert(!dns_data.empty());
      std::string ip = dns_data[(std::uniform_int_distribution<std::size_t>(0, dns_data.size() - 1))(rand_engine)];
      std::cout << "IP: " << ip << '\n';

      std::string path = std::string("/?") + std::to_string(i);
      client.send_https_request(LIBDNS_WITH_IPV6 ? AF_INET6 : AF_INET, ip, "google.com", path, [path](std::vector<std::vector<char>> res) {
        std::string body(res[1].begin(), res[1].end());
        assert(body.find(std::string(path)) != std::string::npos);
      });
    });
  }

  std::string host = "en.wikipedia.org";
  client.query(host, 1, [&client, host](std::vector<std::string> dns_data) {
    assert(!dns_data.empty());
    std::string ip = dns_data[0];

    std::string path = "/w/api.php?action=query&format=json&list=random&rnnamespace=0";
    for (int i = 0; i < 9; i ++) {
      // get wikipedia random title
      client.send_https_request(AF_INET, ip, host, path, [&client, ip, host](std::vector<std::vector<char>> res) {
        std::string body(res[1].begin(), res[1].end());
        assert(!body.empty());
        rapidjson::Document data;
        data.Parse(body.c_str());
        assert(data["query"].IsObject() && data["query"]["random"].IsArray() && data["query"]["random"][0].IsObject());
        std::string title = data["query"]["random"][0]["title"].GetString();
        assert(!title.empty());
        std::cout << "Random title: " << title << '\n';

        // get article of wikipedia random title
        std::string path = "/w/api.php?action=parse&format=json&page=" + libdns::urlencode(title);
        client.send_https_request(AF_INET, ip, host, path, [](std::vector<std::vector<char>> res) {
          std::string body(res[1].begin(), res[1].end());
          assert(!body.empty());
          rapidjson::Document data;
          data.Parse(body.c_str());
          assert(data["parse"].IsObject() && data["parse"]["text"].IsObject() && data["parse"]["text"]["*"].IsString());
          std::string text = data["parse"]["text"]["*"].GetString();
          assert(!text.empty());
          // deal with article
        });
      });
    }
  });


  for (int i = 0; i < 99; i++) { client.receive(9); }

  return 0;
}
