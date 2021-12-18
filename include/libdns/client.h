// created by lccc 12/16/2021, no copyright

#ifndef INCLUDE_LIBDNS_CLIENT_H_
#define INCLUDE_LIBDNS_CLIENT_H_

#include "libdns/config.h"

#include <openssl/ssl.h>

#include <string>
#include <functional>
#include <vector>

#include <sys/epoll.h>

namespace libdns {
  const std::string VERSION = std::to_string(LIBDNS_VERSION_MAJOR) + "." + std::to_string(LIBDNS_VERSION_MINOR);
  const std::string SERVER = LIBDNS_WITH_IPV6 ? LIBDNS_QUERY_SERVER6 : LIBDNS_QUERY_SERVER4;

  /**
   * DNS record types: resource records (RRs)
   * see https://datatracker.ietf.org/doc/html/rfc1035#page-12
   * see https://datatracker.ietf.org/doc/html/rfc3596#section-2
   */
  const std::unordered_map<std::string, std::uint16_t> RRS = {
      { "A", 1 },
      { "NS", 2 },
      { "CNAME", 5 },
      { "PTR", 12 },
      { "MX", 15 },
      { "TXT", 16 },
      { "AAAA", 28 },
      { "SPF", 99 },
  };

  class Client {
   private:
    typedef std::function<void(std::vector<std::string>)> callback_t;

   public:
    explicit Client(std::int8_t log_verbosity_level);
    Client() : Client(0) {};

    /**
     * Query a DNS name Asynchronously
     * @param name domain name
     * @param type DNS record type, 1: A, 28: AAAA ...
     * @param f callback, will give you a answer list
     */
    void query(const std::string& name, std::uint16_t type, const callback_t& f);

    /**
     * Send HTTPS Request
     * a utility tool function, if you need ...
     * @param af AF_INET or AF_INET6
     * @param ip IP address
     * @param host domain name
     * @param path Request Path, like /wiki/Domain_Name_System
     * @param f callback
     */
    void send_https_request(std::int32_t af, const std::string& ip, const std::string& host, const std::string& path, const callback_t& f);

    /**
     * Receive Server Response
     * you need to put this function to an "Event Loop"
     * @param timeout the timeout expires. see https://man7.org/linux/man-pages/man2/epoll_wait.2.html
     */
    void receive(std::int32_t timeout);
    inline void receive() { receive(0); };

   private:
    static constexpr std::size_t HTTP_BUFFER_SIZE = 8192;
    static constexpr int MAX_EVENTS = 1;  // epoll will return for 1 event

    /**
     * epoll file descriptor
     * see https://man7.org/linux/man-pages/man7/epoll.7.html
     */
    int epollfd;
    struct epoll_event events[MAX_EVENTS]{};

    /**
     * SSL configuration context
     * see https://www.openssl.org/docs/man3.0/man3/SSL_CTX_new.html
     */
    SSL_CTX *ssl_ctx;
    std::unordered_map<std::int32_t, SSL*> ssls;

    std::unordered_map<std::int32_t, callback_t> callbacks;
    void process_ssl_response(struct epoll_event event);

    std::int8_t log_verbosity_level;
  };
}

#endif  // INCLUDE_LIBDNS_CLIENT_H_
