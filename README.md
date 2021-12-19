# libdns [![Build Status](https://app.travis-ci.com/lccxz/libdns.svg?branch=main)](https://app.travis-ci.com/lccxz/libdns)
a tiny DNS client library asynchronously(use epoll) for C++

## Example Code
#### See detail: [example/main.cc](https://github.com/lccxz/libdns/blob/main/example/main.cc)
```C++
std::vector<std::pair<std::string, std::string>> params = {  // for test
    { "google.com", "AAAA" },
    { "google.com", "A" },
    { "wikipedia.org", "TXT" },
    { "wikipedia.org", "AAAA" },
    { "en.wikipedia.org", "A" },
};

libdns::Client client;

bool stop = false;

for (const auto& param : params) {
  // query adn get callback
  client.query(param.first, libdns::RRS.at(param.second), [](std::vector<std::string> data) {
    for (const auto& row : data) {
      std::cout << row << '\n';
    }
    
    stop = true;
  });
}

while (!stop) {  // event loop
  client.receive();
}
```

### Output
```
2404:6800:4005:81d::200e
142.251.10.138
103.102.166.224
google-site-verification=AMHkgs-4ViEvIJf5znZle-BSE2EPNFqM1nDJGRyn2qk
v=spf1 include:wikimedia.org ~all
2001:df2:e500:ed1a::1
```


## Integration
### cmake
```
# clone or download this project and add it to CMakeLists.txt
add_subdirectory(libdns)

target_link_libraries(${PROJECT_NAME} PRIVATE libdns)
```

