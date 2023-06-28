#ifndef THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_BUILDER_H_
#define THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_BUILDER_H_

#include <sstream>

#include "src/deps/status.h"

namespace util {

class StatusBuilder {
 public:
  explicit StatusBuilder(error::Code code, int code_location = 0)
      : code_(code) {}

  template <typename T>
  StatusBuilder &operator<<(const T &value) {
    os_ << value;
    return *this;
  }

  operator Status() const { return Status(code_, os_.str()); }

 private:
  error::Code code_;
  std::ostringstream os_;
};

}  // namespace util

#endif  // THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_BUILDER_H_
