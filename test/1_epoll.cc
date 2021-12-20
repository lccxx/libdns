// created by lccc 12/20/2021, no copyright

#include <sys/epoll.h>

int main() {
  int epollfd = epoll_create1(0);

  for (int i = 0; i < 99; i++) {
    epoll_wait(epollfd, {}, 1, 9);
  }
}