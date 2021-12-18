// created by lccc 12/16/2021, no copyright

#include "libdns/client.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <unordered_map>
#include <utility>
#include <regex>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const std::unordered_map<std::int32_t, std::string> AF_MAP = { { AF_INET, "IPv4" }, { AF_INET6, "IPv6"} };

int connect_sock_addr(int epollfd, int sockfd, const struct sockaddr *sock_addr, std::size_t sock_addr_len) {
  if (connect(sockfd, sock_addr, sock_addr_len) == -1) {
    perror("socket connect");
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
    close(sockfd);
    return -1;
  }
  return sockfd;
}

int connect_ip(int epollfd, std::int32_t af, const std::string& ip_addr, int port) {
  int sockfd;
  if ((sockfd = socket(af, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    std::cout << AF_MAP.at(af) << " socket create error" << '\n';
    return -1;
  }
  struct epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = sockfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
    perror("epoll_ctl: ");
    close(sockfd);
    return -1;
  }
  if (af == AF_INET) {
    struct sockaddr_in sock_addr_v4{};
    sock_addr_v4.sin_family = af;
    sock_addr_v4.sin_port = htons(port);
    inet_pton(af, ip_addr.c_str(), &sock_addr_v4.sin_addr);
    std::size_t sock_addr_len = sizeof(sock_addr_v4);
    return connect_sock_addr(epollfd, sockfd, (struct sockaddr*)(&sock_addr_v4), sock_addr_len);
  } else if (af == AF_INET6) {
    struct sockaddr_in6 sock_addr_v6{};
    sock_addr_v6.sin6_family = af;
    sock_addr_v6.sin6_port = htons(port);
    inet_pton(af, ip_addr.c_str(), &sock_addr_v6.sin6_addr);
    std::size_t sock_addr_len = sizeof(sock_addr_v6);
    return connect_sock_addr(epollfd, sockfd, (struct sockaddr*)(&sock_addr_v6), sock_addr_len);
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
    perror("connect");
    return;
  }

  SSL *ssl = SSL_new(ssl_ctx);
  do {
    if (!ssl) {
      printf("Error creating SSL.\n");
      break;
    }
    if (SSL_set_fd(ssl, sockfd) == 0) {
      printf("Error to set fd.\n");
      break;
    }
    int err = SSL_connect(ssl);
    if (err <= 0) {
      printf("Error creating SSL connection.  err=%x\n", err);
      break;
    }
    if (log_verbosity_level > 0) {
      printf("SSL connection using %s\n", SSL_get_cipher(ssl));
    }

    std::stringstream req;
    req << "GET " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";
    req << "User-Agent: tdscript/" << VERSION << "\r\n";
    req << "Accept: */*\r\n";
    req << "\r\n";

    auto data = req.str();
    if (SSL_write(ssl, data.c_str(), data.length()) < 0) {  // NOLINT(cppcoreguidelines-narrowing-conversions)
      break;
    }

    if (log_verbosity_level > 0) {
      std::cout << "HTTPS request: " << data << '\n';
    }
    callbacks[sockfd] = f;
    ssls[sockfd] = ssl;
    return;
  } while (false);

  epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
  close(sockfd);
  SSL_free(ssl);
}

void libdns::Client::query(const std::string& name, std::uint16_t type, const callback_t& f) {
  std::int32_t af = LIBDNS_WITH_IPV6 ? AF_INET6 : AF_INET;
  std::string query = "/resolve?name=" + name + "&type=" + std::to_string(type);
  send_https_request(af, SERVER, "dns.google", query, [type, f](std::vector<std::string> res) {
    std::vector<std::string> result;
    try {
      auto data = nlohmann::json::parse(res[1]);
      if (data["Status"] == 0) {
        for (const auto &row : data["Answer"]) {
          if (row["type"] == type) {
            result.push_back(row["data"]);
          }
        }
      }
    } catch (nlohmann::json::parse_error &ex) { }
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

  std::string res, head, body;
  char buffer[HTTP_BUFFER_SIZE];
  int response_size;
  std::uint64_t content_length = 0;
  bool chunked = false;
  std::regex length_regex("\r\nContent-Length: (\\d+)\r\n", std::regex_constants::icase);
  std::regex chunked_regex("\r\nTransfer-Encoding: chunked\r\n", std::regex_constants::icase);
  std::size_t blank_line_pos = std::string::npos;
  do {
    if ((response_size = SSL_read(ssl, buffer, HTTP_BUFFER_SIZE)) <= 0) {
      perror("recv");
      break;
    }
    res.append(buffer, 0, response_size);
    if (content_length == 0 && !chunked) {
      std::smatch length_match;
      std::smatch chunked_match;
      if (std::regex_search(res, length_match, length_regex)) {
        if (length_match.size() == 2) {
          content_length = std::stoull(length_match[1]);
        }
      } else if (std::regex_search(res, chunked_match, chunked_regex)) {
        chunked = true;
      }
    }
    if (blank_line_pos == std::string::npos) {
      blank_line_pos = res.find("\r\n\r\n");
    }
    if (blank_line_pos != std::string::npos) {
      if (content_length > 0) {
        if (res.length() - (blank_line_pos + 4) >= content_length) {
          break;
        }
      } else if (chunked) {
        if ((res.find("0\r\n\r\n") + 5) == res.length()) {
          break;
        }
      }
    }
  } while(true);

  head = res.substr(0, blank_line_pos);

  if (chunked) {
    std::vector<std::string> chunks;
    std::size_t chunk_start_pos = blank_line_pos + 4, chunk_data_start_pos, chunk_size;
    do {
      chunk_data_start_pos = res.find("\r\n", chunk_start_pos) + 2;
      chunk_size = std::stoull(res.substr(chunk_start_pos, chunk_data_start_pos - chunk_start_pos), nullptr, 16);
      body.append(res, chunk_data_start_pos, chunk_size);
      chunk_start_pos = chunk_data_start_pos + chunk_size + 2;
    } while(chunk_size > 0 && chunk_start_pos < res.length() - 1);
  } else if (content_length > 0) {
    body = res.substr(blank_line_pos + 4, content_length);
  }

  if (log_verbosity_level > 0) {
    std::cout << "ssl socket(" << sockfd << ") response: " << head << "\n\n" << body << '\n';
  }
  callbacks[sockfd]({ head, body });
  callbacks.erase(sockfd);
  epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &event);
  close(sockfd);
  SSL_free(ssl);
}