// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include "clockcore/ImageSet.h"

using namespace clockcore;

void
setUp()
{
}
void
tearDown()
{
}

void
test_set_names()
{
  TEST_ASSERT_TRUE(isValidSetName("nixie"));
  TEST_ASSERT_TRUE(isValidSetName("blue-ribbon-2"));
  TEST_ASSERT_TRUE(isValidSetName("abcdefghijklmnopqrst"));   // 20
  TEST_ASSERT_FALSE(isValidSetName("abcdefghijklmnopqrstu")); // 21
  TEST_ASSERT_FALSE(isValidSetName(""));
  TEST_ASSERT_FALSE(isValidSetName("Nixie"));
  TEST_ASSERT_FALSE(isValidSetName("nix_ie"));
  TEST_ASSERT_FALSE(isValidSetName("nix ie"));
}

void
test_parse_file_names()
{
  ImageName n;
  TEST_ASSERT_TRUE(parseImageFileName("nixie_3_7.png", n));
  TEST_ASSERT_EQUAL_STRING("nixie", n.set.c_str());
  TEST_ASSERT_EQUAL(3, n.screen);
  TEST_ASSERT_EQUAL(7, n.digit);
  TEST_ASSERT_TRUE(parseImageFileName("/a_0_0.png", n));
  TEST_ASSERT_EQUAL_STRING("a", n.set.c_str());

  TEST_ASSERT_FALSE(parseImageFileName("nixie_6_0.png", n)); // screen out of range
  TEST_ASSERT_FALSE(parseImageFileName("nixie_0_10.png", n));
  TEST_ASSERT_FALSE(parseImageFileName("nixie_0_1.bmp", n));
  TEST_ASSERT_FALSE(parseImageFileName("nixie_0_1.PNG", n));
  TEST_ASSERT_FALSE(parseImageFileName("_0_1.png", n));
  TEST_ASSERT_FALSE(parseImageFileName("10.bmp", n));
  TEST_ASSERT_FALSE(parseImageFileName("config.json", n));
  TEST_ASSERT_EQUAL_STRING("nixie_5_9.png", imageFileName("nixie", 5, 9).c_str());
}

void
test_fallback_to_screen_zero()
{
  ImageSetIndex idx;
  for (int d = 0; d < 10; ++d)
    TEST_ASSERT_TRUE(idx.add(imageFileName("plain", 0, d)));
  TEST_ASSERT_TRUE(idx.isComplete("plain"));
  TEST_ASSERT_EQUAL_STRING("plain_0_4.png", idx.resolve("plain", 5, 4).c_str());
  // A screen specific override wins.
  idx.add("plain_5_4.png");
  TEST_ASSERT_EQUAL_STRING("plain_5_4.png", idx.resolve("plain", 5, 4).c_str());
  TEST_ASSERT_EQUAL_STRING("plain_0_4.png", idx.resolve("plain", 4, 4).c_str());
}

void
test_incomplete_sets_are_not_offered()
{
  ImageSetIndex idx;
  for (int d = 0; d < 9; ++d) // digit 9 missing
    idx.add(imageFileName("broken", 0, d));
  for (int s = 1; s < 6; ++s)
    idx.add(imageFileName("broken", s, 9)); // ...except on screen 0
  for (int s = 0; s < 6; ++s)
    for (int d = 0; d < 10; ++d)
      idx.add(imageFileName("full", s, d));
  idx.add("ignored.txt");

  TEST_ASSERT_FALSE(idx.isComplete("broken"));
  TEST_ASSERT_EQUAL_STRING("", idx.resolve("broken", 0, 9).c_str());
  TEST_ASSERT_EQUAL_STRING("broken_1_9.png", idx.resolve("broken", 1, 9).c_str());

  std::vector<std::string> complete = idx.completeSets();
  TEST_ASSERT_EQUAL(1, int(complete.size()));
  TEST_ASSERT_EQUAL_STRING("full", complete[0].c_str());
  TEST_ASSERT_EQUAL(2, int(idx.allSets().size()));
}

void
test_remove()
{
  ImageSetIndex idx;
  idx.add("x_0_1.png");
  idx.add("x_0_2.png");
  idx.remove("x_0_1.png");
  TEST_ASSERT_EQUAL_STRING("", idx.resolve("x", 0, 1).c_str());
  TEST_ASSERT_EQUAL(1, int(idx.allSets().size()));
  idx.remove("x_0_2.png");
  TEST_ASSERT_EQUAL(0, int(idx.allSets().size()));
  TEST_ASSERT_EQUAL_STRING("", idx.resolve("unknown", 0, 0).c_str());
}

int
main(int, char**)
{
  UNITY_BEGIN();
  RUN_TEST(test_set_names);
  RUN_TEST(test_parse_file_names);
  RUN_TEST(test_fallback_to_screen_zero);
  RUN_TEST(test_incomplete_sets_are_not_offered);
  RUN_TEST(test_remove);
  return UNITY_END();
}
