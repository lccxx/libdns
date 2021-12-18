# libdns
a tiny DNS client library asynchronously(use epoll) for C++

## Example Code
```C++
std::vector<std::pair<std::string, std::string>> params = {  // for test
    { "google.com", "AAAA" },
    { "google.com", "A" },
    { "wikipedia.org", "TXT" },
    { "wikipedia.org", "AAAA" },
    { "en.wikipedia.org", "A" },
};

std::uint32_t done_check = 0;  // for test


libdns::Client client;

for (const auto& param : params) {
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

while (!stop) {  // event loop
  client.receive();
}
```

### Output
```
google.com, AAAA -> 2404:6800:4005:81d::200e
google.com, A -> 172.217.31.238
en.wikipedia.org, A -> 103.102.166.224
wikipedia.org, TXT -> google-site-verification=AMHkgs-4ViEvIJf5znZle-BSE2EPNFqM1nDJGRyn2qk
wikipedia.org, TXT -> v=spf1 include:wikimedia.org ~all
wikipedia.org, AAAA -> 2001:df2:e500:ed1a::1
```
