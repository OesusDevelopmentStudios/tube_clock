// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include "../fixtures/png_fixtures.h"
#include "clockcore/Png565.h"

using namespace clockcore;

void
setUp()
{
}
void
tearDown()
{
}

static constexpr uint16_t RED = 0xF800, GREEN = 0x07E0, BLUE = 0x001F, WHITE = 0xFFFF, BLACK = 0;

void
test_rgb_exact_size()
{
  uint16_t fb[2 * 2];
  Png565Decoder dec(fb, 2, 2);
  TEST_ASSERT_TRUE_MESSAGE(dec.decode(kPngRgb2x2, sizeof(kPngRgb2x2)), dec.error());
  TEST_ASSERT_EQUAL(2, dec.imageWidth());
  TEST_ASSERT_EQUAL_HEX16(RED, fb[0]);
  TEST_ASSERT_EQUAL_HEX16(GREEN, fb[1]);
  TEST_ASSERT_EQUAL_HEX16(BLUE, fb[2]);
  TEST_ASSERT_EQUAL_HEX16(WHITE, fb[3]);
}

void
test_palette_centered()
{
  uint16_t fb[5 * 3];
  for (auto& p : fb)
    p = 0x1234; // must be cleared
  Png565Decoder dec(fb, 5, 3);
  TEST_ASSERT_TRUE_MESSAGE(dec.decode(kPngPalette3x1, sizeof(kPngPalette3x1)), dec.error());
  // 3x1 image in a 5x3 frame -> offset (1, 1)
  const uint16_t expected[5 * 3] = { BLACK, BLACK, BLACK, BLACK, BLACK, //
                                     BLACK, BLUE,  GREEN, RED,   BLACK, //
                                     BLACK, BLACK, BLACK, BLACK, BLACK };
  TEST_ASSERT_EQUAL_HEX16_ARRAY(expected, fb, 15);
}

void
test_larger_image_is_cropped()
{
  uint16_t fb[1];
  Png565Decoder dec(fb, 1, 1);
  TEST_ASSERT_TRUE(dec.decode(kPngRgb2x2, sizeof(kPngRgb2x2)));
  // offset (2-1)/2 = 0 after integer division of -1/2 -> 0: top-left pixel lands on (0,0)
  TEST_ASSERT_EQUAL_HEX16(RED, fb[0]);
}

void
test_alpha_and_brightness()
{
  uint16_t fb[2];
  Png565Decoder dec(fb, 2, 1);
  TEST_ASSERT_TRUE(dec.decode(kPngRgba2x1, sizeof(kPngRgba2x1)));
  TEST_ASSERT_EQUAL_HEX16(BLACK, fb[0]); // fully transparent white -> black
  TEST_ASSERT_EQUAL_HEX16(rgb565(200, 100, 50), fb[1]);

  TEST_ASSERT_TRUE(dec.decode(kPngRgba2x1, sizeof(kPngRgba2x1), 128));
  TEST_ASSERT_EQUAL_HEX16(rgb565(100, 50, 25), fb[1]);
  TEST_ASSERT_TRUE(dec.decode(kPngRgb2x2, sizeof(kPngRgb2x2), 0));
  TEST_ASSERT_EQUAL_HEX16(BLACK, fb[0]);
}

void
test_streaming_in_small_chunks()
{
  uint16_t fb[4];
  Png565Decoder dec(fb, 2, 2);
  TEST_ASSERT_TRUE(dec.begin());
  for (size_t i = 0; i < sizeof(kPngRgb2x2); i += 7) {
    size_t n = sizeof(kPngRgb2x2) - i < 7 ? sizeof(kPngRgb2x2) - i : 7;
    TEST_ASSERT_TRUE_MESSAGE(dec.feed(kPngRgb2x2 + i, n), dec.error());
  }
  dec.end();
  TEST_ASSERT_EQUAL_HEX16(WHITE, fb[3]);
}

void
test_rejects_garbage()
{
  uint16_t fb[4];
  Png565Decoder dec(fb, 2, 2);
  const uint8_t junk[] = { 'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0 };
  TEST_ASSERT_FALSE(dec.decode(junk, sizeof(junk)));
  TEST_ASSERT_NOT_NULL(dec.error());
  TEST_ASSERT_FALSE(dec.decode(kPngRgb2x2, 8)); // signature only, no header
}

int
main(int, char**)
{
  UNITY_BEGIN();
  RUN_TEST(test_rgb_exact_size);
  RUN_TEST(test_palette_centered);
  RUN_TEST(test_larger_image_is_cropped);
  RUN_TEST(test_alpha_and_brightness);
  RUN_TEST(test_streaming_in_small_chunks);
  RUN_TEST(test_rejects_garbage);
  return UNITY_END();
}
