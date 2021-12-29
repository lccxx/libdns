// created by lccc 12/16/2021, no copyright

#include "libdns/client.h"

#include "rapidjson/document.h"

#include <iostream>
#include <unordered_map>
#include <utility>
#include <regex>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const std::vector<std::string> HEX_CODES = { "0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F" };
const std::unordered_map<std::int32_t, std::string> AF_MAP = { { AF_INET, "IPv4" }, { AF_INET6, "IPv6"} };

int connect_sock(int epollfd, int sockfd, const struct sockaddr *sock_addr, std::size_t sock_addr_len) {
  if (connect(sockfd, sock_addr, sock_addr_len) == -1) {
    std::cerr << "socket connect error" << std::endl;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
    close(sockfd);
    return -1;
  }
  return sockfd;
}

int connect_ip(int epollfd, std::int32_t af, const std::string& ip_addr, int port) {
  int sockfd;
  if ((sockfd = socket(af, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    std::cerr << AF_MAP.at(af) << " socket create error" << std::endl;
    return -1;
  }
  struct epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = sockfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
    std::cerr << "epoll ctl add error" << std::endl;
    close(sockfd);
    return -1;
  }
  if (af == AF_INET) {
    struct sockaddr_in sock_addr_v4{};
    sock_addr_v4.sin_family = af;
    sock_addr_v4.sin_port = htons(port);
    inet_pton(af, ip_addr.c_str(), &sock_addr_v4.sin_addr);
    std::size_t sock_addr_len = sizeof(sock_addr_v4);
    return connect_sock(epollfd, sockfd, (struct sockaddr *) (&sock_addr_v4), sock_addr_len);
  } else if (af == AF_INET6) {
    struct sockaddr_in6 sock_addr_v6{};
    sock_addr_v6.sin6_family = af;
    sock_addr_v6.sin6_port = htons(port);
    inet_pton(af, ip_addr.c_str(), &sock_addr_v6.sin6_addr);
    std::size_t sock_addr_len = sizeof(sock_addr_v6);
    return connect_sock(epollfd, sockfd, (struct sockaddr *) (&sock_addr_v6), sock_addr_len);
  }

  epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &event);
  close(sockfd);
  return -1;
}

libdns::Client::Client(std::int8_t log_verbosity_level) {
  epollfd = epoll_create1(0);

  SSL_library_init();
  SSLeay_add_ssl_algorithms();
  SSL_load_error_strings();
  ssl_ctx = SSL_CTX_new(TLS_client_method());

  this->log_verbosity_level = log_verbosity_level;
}

void libdns::Client::send_https_request(std::int32_t af, const std::string& ip, const std::string& host, const std::string& path, const callback_t& f) {
  int sockfd;
  if ((sockfd = connect_ip(epollfd, af, ip, 443)) == -1) {
    std::cerr << "connect error" << std::endl;
    return;
  }

  SSL *ssl = SSL_new(ssl_ctx);
  do {
    if (!ssl) {
      std::cerr << "Error creating SSL." << std::endl;
      break;
    }
    if (SSL_set_fd(ssl, sockfd) == 0) {
      std::cerr << "Error to set fd." << std::endl;
      break;
    }
    int err = SSL_connect(ssl);
    if (err <= 0) {
      std::cerr << "Error creating SSL connection. err = " << err << std::endl;
      break;
    }
    if (log_verbosity_level > 0) {
      std::cout << "SSL connection using " << SSL_get_cipher(ssl) << std::endl;
    }

    std::stringstream req;
    req << "GET " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";
    req << "User-Agent: libdns/" << VERSION << "\r\n";
    req << "Accept: */*\r\n";
    req << "\r\n";

    auto data = req.str();
    if (SSL_write(ssl, data.c_str(), data.length()) < 0) {  // NOLINT(cppcoreguidelines-narrowing-conversions)
      break;
    }

    if (log_verbosity_level > 0) {
      std::cout << "HTTPS request " << ip << ": " << data << std::endl;
    }
    callbacks[sockfd] = f;
    ssls[sockfd] = ssl;
    return;
  } while (false);  // NOLINT

  epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
  close(sockfd);
  SSL_free(ssl);
}

void libdns::Client::query(const std::string& name, std::uint16_t type, const std::function<void(std::vector<std::string>)>& f) {
  std::int32_t af = LIBDNS_WITH_IPV6 ? AF_INET6 : AF_INET;
  std::string query = "/resolve?name=" + name + "&type=" + std::to_string(type);
  send_https_request(af, SERVER, "dns.google", query, [type, f](std::vector<std::vector<char>> res) {
    std::vector<std::string> result;
    rapidjson::Document data;
    data.Parse(std::string(res[1].begin(), res[1].end()).c_str());
    if (data["Status"].IsInt() && data["Status"].GetInt() == 0 && data["Answer"].IsArray()) {
      for (rapidjson::SizeType i = 0; i < data["Answer"].Size(); i++) {
        const auto& row = data["Answer"][i].GetObject();
        if (row["type"].GetInt() == type) {
          result.emplace_back(row["data"].GetString());
        }
      }
    }
    f(result);
  });
}

void libdns::Client::receive(std::int32_t timeout) {
  if (epoll_wait(epollfd, events, MAX_EVENTS, timeout) > 0) {
    process_ssl_response(events[0]);
  }
}

void libdns::Client::process_ssl_response(struct epoll_event event) {
  int sockfd = event.data.fd;
  SSL *ssl = ssls[sockfd];

  std::vector<char> data;
  std::string head;
  char buffer[HTTP_BUFFER_SIZE];
  int response_size;
  std::uint64_t content_length = 0;
  bool chunked = false;
  std::regex length_regex("\r\nContent-Length: (\\d+)", std::regex_constants::icase);
  std::regex chunked_regex("\r\nTransfer-Encoding: chunked", std::regex_constants::icase);
  long blank_line_pos = -1;
  do {
    if ((response_size = SSL_read(ssl, buffer, HTTP_BUFFER_SIZE)) <= 0) {
      std::cerr << "ssl read error: " << response_size << std::endl;
      break;
    }
    if (log_verbosity_level > 0) {
      std::cout << "ssl read size: " << response_size
                << ", content-length: " << content_length
                << ", chunked: " << chunked
                << ", blank line position: " << blank_line_pos
                << ", data size: " << data.size()
                << ", head size: " << head.size()
                << std::endl;
    }
    for (int i = 0; i < response_size; i++) { data.push_back(buffer[i]); }

    if (blank_line_pos == -1) {
      for (int i = 0; i < data.size() - 3; i++) {
        if (data[i] == '\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n') {
          blank_line_pos = i;
          break;
        }
      }
    }

    if (blank_line_pos != -1) {
      if (head.empty()) {
        head = std::string(data.begin(), data.begin() + blank_line_pos);
      }

      if (content_length == 0 && !chunked) {
        std::smatch length_match;
        if (std::regex_search(head, length_match, length_regex)) {
          content_length = std::stoull(length_match[1]);
        } else if (std::regex_search(head, chunked_regex)) {
          chunked = true;
        }
      }

      if (content_length > 0) {
        if (data.size() - (blank_line_pos + 4) >= content_length) {
          break;
        }
      } else if (chunked) {
        if (data[data.size() - 1] == '\n' && data[data.size() - 2] == '\r'
            && data[data.size() - 3] == '\n' && data[data.size() - 4] == '\r'
            && data[data.size() - 5] == '0') {
          break;
        }
      }
    }
  } while(true);

  if (log_verbosity_level > 0) {
    std::cout << "ssl socket(" << sockfd << ") response: " << head << '\n' << std::endl;
  }

  std::vector<char> body;
  if (chunked) {
    long chunk_start_pos = blank_line_pos + 4, chunk_data_start_pos = 0, chunk_size;
    do {
      for (long i = chunk_start_pos; i < data.size() - 1; i++) {
        if (data[i] == '\r' && data[i+1] == '\n') {
          chunk_data_start_pos = i + 2;
          break;
        }
      }
      chunk_size = std::stol(std::string(data.begin() + chunk_start_pos, data.begin() + chunk_data_start_pos - 1), nullptr, 16);
      if (chunk_size == 0) {
        break;
      }
      body.insert(body.end(), data.begin() + chunk_data_start_pos, data.begin() + chunk_data_start_pos + chunk_size);
      chunk_start_pos = chunk_data_start_pos + chunk_size + 2;
    } while(chunk_size > 0 && chunk_start_pos < data.size() - 1);
  } else if (content_length > 0) {
    body.insert(body.begin(), data.begin() + blank_line_pos + 4, data.end());
  }

  if (log_verbosity_level > 0) {
    std::cout << std::string(body.begin(), body.end()) << std::endl;
  }

  callbacks[sockfd]({ std::vector<char>(head.begin(), head.end()), body });
  callbacks.erase(sockfd);
  epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &event);
  close(sockfd);
  SSL_free(ssl);
}

std::string libdns::urlencode(const std::string& str) {
  std::string encode;

  const char *s = str.c_str();
  for (int i = 0; s[i]; i++) {
    char ci = s[i];
    if ((ci >= 'a' && ci <= 'z') ||
        (ci >= 'A' && ci <= 'Z') ||
        (ci >= '0' && ci <= '9') ) { // allowed
      encode.push_back(ci);
    } else if (ci == ' ') {
      encode.push_back('+');
    } else {
      encode.append("%").append(char_to_hex(ci));
    }
  }

  return encode;
}

std::string libdns::char_to_hex(char c) {
  std::uint8_t n = c;
  std::string res;
  res.append(HEX_CODES[n / 16]);
  res.append(HEX_CODES[n % 16]);
  return res;
}
