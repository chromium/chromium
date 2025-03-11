#ifndef icu4x_WeekendContainsDay_D_HPP
#define icu4x_WeekendContainsDay_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct WeekendContainsDay {
      bool monday;
      bool tuesday;
      bool wednesday;
      bool thursday;
      bool friday;
      bool saturday;
      bool sunday;
    };
    
    typedef struct WeekendContainsDay_option {union { WeekendContainsDay ok; }; bool is_ok; } WeekendContainsDay_option;
} // namespace capi
} // namespace


namespace icu4x {
struct WeekendContainsDay {
  bool monday;
  bool tuesday;
  bool wednesday;
  bool thursday;
  bool friday;
  bool saturday;
  bool sunday;

  inline icu4x::capi::WeekendContainsDay AsFFI() const;
  inline static icu4x::WeekendContainsDay FromFFI(icu4x::capi::WeekendContainsDay c_struct);
};

} // namespace
#endif // icu4x_WeekendContainsDay_D_HPP
