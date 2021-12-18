// created by lccc 12/19/2021, no copyright

#include "libdns/client.h"

#include "gtest/gtest.h"

TEST(RandomTest, Create) {   // NOLINT(cert-err58-cpp)
  EXPECT_EQ("0.1", libdns::VERSION) << "version";

  bool stop = false;

  libdns::Client client;

  client.query("one.one.one.one", libdns::RRS.at("A"), [&stop](const std::vector<std::string>& data) {
    stop = true;

    bool found = false;
    for (const auto& row : data) {
      if ("1.1.1.1" == row) {
        found = true;
      }
    }

    ASSERT_TRUE(found);
  });

  while (!stop) { client.receive(); }
}