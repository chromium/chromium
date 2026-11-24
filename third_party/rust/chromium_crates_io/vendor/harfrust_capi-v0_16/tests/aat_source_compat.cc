// Blink stores AAT feature types in an unsigned vector and compares them with
// these constants in a template. Integer macros must retain unsigned types.
#include "hr-hb-aat.h"
#include <type_traits>

static_assert(std::is_same<decltype(HB_AAT_LAYOUT_FEATURE_TYPE_LETTER_CASE),
                           hb_aat_layout_feature_type_t>::value, "feature type");
static_assert(std::is_same<decltype(HB_AAT_LAYOUT_FEATURE_SELECTOR_SMALL_CAPS),
                           hb_aat_layout_feature_selector_t>::value, "selector");
static_assert(std::is_same<decltype(HB_AAT_LAYOUT_NO_SELECTOR_INDEX),
                           unsigned int>::value, "unset selector index");

template <typename Value>
bool contains(hb_aat_layout_feature_type_t feature, Value value) {
  return feature == value;
}

bool has_small_caps(hb_aat_layout_feature_type_t feature) {
  return contains(feature, HB_AAT_LAYOUT_FEATURE_TYPE_LETTER_CASE) ||
         contains(feature, HB_AAT_LAYOUT_FEATURE_TYPE_LOWER_CASE) ||
         contains(feature, HB_AAT_LAYOUT_FEATURE_TYPE_UPPER_CASE);
}
