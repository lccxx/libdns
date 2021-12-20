// created by lccc 12/20/2021, no copyright

#include <openssl/ssl.h>

#include <iostream>

int main() {
  SSL_library_init();
  SSLeay_add_ssl_algorithms();
  SSL_load_error_strings();
  auto ssl_ctx = SSL_CTX_new(TLS_client_method());
  auto ssl = SSL_new(ssl_ctx);
  if (!ssl) {
    std::cerr << "Error creating SSL.\n";
    return 1;
  }
}