// Chromium uses these macros in bool aggregate initializers and passes its
// Latin-1 byte spans without a cast. Keep the HarfBuzz source types intact.
#include "hr-hb.h"
#include "hr-hb-ot.h"
#include <type_traits>

static_assert(std::is_same<decltype(HB_TAG_NONE), hb_tag_t>::value,
              "unset tags have the unsigned tag type");
static_assert(std::is_same<decltype(HB_FEATURE_GLOBAL_START), unsigned int>::value,
              "feature range bounds are unsigned");
static_assert(std::is_same<decltype(HB_BUFFER_FLAG_DEFAULT), hb_buffer_flags_t>::value,
              "buffer flags have their declared unsigned type");
static_assert(std::is_same<decltype(HB_OT_TAG_MATH), hb_tag_t>::value,
              "OpenType tags have the unsigned tag type");
static_assert(std::is_same<decltype(HB_OT_MATH_GLYPH_PART_FLAG_EXTENDER),
                           hb_ot_math_glyph_part_flags_t>::value,
              "MATH glyph flags have their declared unsigned type");

template <typename Left, typename Right>
bool equal(Left left, Right right) {
  return left == right;
}

bool tag_none_is_zero() {
  // Chromium's DCHECK_EQ instantiates a similar comparison with -Wsign-compare.
  return equal(HB_TAG_NONE, 0u);
}

static_assert(HB_DIRECTION_IS_VALID(HB_DIRECTION_LTR), "valid direction");
static_assert(!HB_DIRECTION_IS_VALID(HB_DIRECTION_INVALID), "invalid direction");
static_assert(HB_DIRECTION_IS_HORIZONTAL(HB_DIRECTION_RTL), "horizontal");
static_assert(HB_DIRECTION_IS_VERTICAL(HB_DIRECTION_TTB), "vertical");
static_assert(HB_DIRECTION_IS_FORWARD(HB_DIRECTION_TTB), "forward");
static_assert(HB_DIRECTION_IS_BACKWARD(HB_DIRECTION_BTT), "backward");
static_assert(HB_DIRECTION_REVERSE(HB_DIRECTION_LTR) == HB_DIRECTION_RTL, "reverse");
static_assert(std::is_same<decltype(HB_DIRECTION_IS_HORIZONTAL(HB_DIRECTION_LTR)),
                           bool>::value, "direction predicates return bool");

using AddLatin1 = void (*)(hb_buffer_t *, const uint8_t *, int, unsigned int, int);
static_assert(std::is_same<decltype(&hb_buffer_add_latin1), AddLatin1>::value,
              "Latin-1 accepts unsigned bytes");

struct Orientation {
  bool horizontal;
};

Orientation orientation(hb_direction_t direction) {
  return {HB_DIRECTION_IS_HORIZONTAL(direction)};
}
