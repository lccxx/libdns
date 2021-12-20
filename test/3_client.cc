// created by lccc 12/20/2021, no copyright

#include "libdns/client.h"

int main() {
  libdns::Client client;

  for (int i = 0; i < 9; i++) {
    client.receive();
  }
}