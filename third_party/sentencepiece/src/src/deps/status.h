#ifndef THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_H_
#define THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_H_

#include <memory>
#include <string>

namespace util {
namespace error {
enum Code {
  OK = 0,
  CANCELLED = 1,
  UNKNOWN = 2,
  INVALID_ARGUMENT = 3,
  DEADLINE_EXCEEDED = 4,
  NOT_FOUND = 5,
  ALREADY_EXISTS = 6,
  PERMISSION_DENIED = 7,
  UNAUTHENTICATED = 16,
  RESOURCE_EXHAUSTED = 8,
  FAILED_PRECONDITION = 9,
  ABORTED = 10,
  OUT_OF_RANGE = 11,
  UNIMPLEMENTED = 12,
  INTERNAL = 13,
  UNAVAILABLE = 14,
  DATA_LOSS = 15,
};
}  // namespace error

class Status {
 public:
  Status();
  ~Status();
  Status(error::Code code, const char *error_message);
  Status(error::Code code, const std::string &error_message);
  Status(const Status &s);
  Status &operator=(const Status &s);
  bool operator==(const Status &s) const;
  bool operator!=(const Status &s) const;
  inline bool ok() const { return rep_ == nullptr; }

  void set_error_message(const char *str);
  const char *message() const;
  error::Code code() const;
  std::string ToString() const;

  void IgnoreError();

 private:
  struct Rep;
  std::unique_ptr<Rep> rep_;
};

inline Status OkStatus() { return Status(); }

}  // namespace util

#define CHECK_OK(expr)                         \
  do {                                         \
    const auto _status = expr;                 \
    CHECK(_status.ok()) << _status.ToString(); \
  } while (0)

#define CHECK_NOT_OK(expr)                      \
  do {                                          \
    const auto _status = expr;                  \
    CHECK(!_status.ok()) << _status.ToString(); \
  } while (0)

#endif  // THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_H_
