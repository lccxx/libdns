// created by lccc 12/19/2021, no copyright

#include "libdns/client.h"
#include "rapidjson/document.h"

#include <cassert>
#include <iostream>

#include <arpa/inet.h>

int main() {
  auto client = libdns::Client(0);

  std::string host = "en.wikipedia.org";
  client.query(host, 1, [&client, host](std::vector<std::string> data) {
    assert(!data.empty());
    std::string ip = data[0];

    std::string path = "/w/api.php?action=query&format=json&list=random&rnnamespace=0";
    for (int i = 0; i < 9; i ++) {
      // get wikipedia random title
      client.send_https_request(AF_INET, ip, host, path, [&client, ip, host](std::vector<std::string> res) {
        std::string body = res[1];
        assert(!body.empty());
        rapidjson::Document data;
        data.Parse(body.c_str());
        assert(data["query"].IsObject() && data["query"]["random"].IsArray() && data["query"]["random"][0].IsObject());
        std::string title = data["query"]["random"][0]["title"].GetString();
        assert(!title.empty());
        std::cout << "Random title: " << title << '\n';

        // get article of wikipedia random title
        std::string path = "/w/api.php?action=parse&format=json&page=" + libdns::urlencode(title);
        client.send_https_request(AF_INET, ip, host, path, [](std::vector<std::string> res) {
          std::string body = res[1];
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

  for (int i = 0; i < 9; i ++) {
    client.query("google.com", 1, [&client, i](std::vector<std::string> data) {
      assert(!data.empty());
      std::string ip = data[0];
      std::string path = std::string("/?") + std::to_string(i);
      client.send_https_request(AF_INET, ip, "google.com", path, [path](std::vector<std::string> res) {
        std::string body = res[1];
        assert(body.find(std::string(path)) != std::string::npos);
      });
    });
  }

  for (int i = 0; i < 99; i++) { client.receive(9); }

  return 0;
}