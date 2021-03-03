#include "gtest/gtest.h"

#include "iotea.hpp"

TEST(talent, id_matches_constructed) {
  auto talent = iotea::core::Talent("test_talent", nullptr);
  ASSERT_STREQ(talent.GetId().c_str(), "test_talent");
}
