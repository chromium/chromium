#ifndef THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_CANONICAL_ERRORS_H_
#define THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_CANONICAL_ERRORS_H_

#include <string>

#include "absl/strings/string_view.h"
#include "src/deps/status.h"

namespace util {

#define DECLARE_ERROR(FUNC, CODE)                            \
  inline ::util::Status FUNC##Error(absl::string_view str) { \
    return ::util::Status(error::CODE, str.data());          \
  }                                                          \
  inline bool Is##FUNC(const ::util::Status &status) {       \
    return status.code() == error::CODE;                     \
  }

DECLARE_ERROR(Cancelled, CANCELLED)
DECLARE_ERROR(InvalidArgument, INVALID_ARGUMENT)
DECLARE_ERROR(NotFound, NOT_FOUND)
DECLARE_ERROR(AlreadyExists, ALREADY_EXISTS)
DECLARE_ERROR(ResourceExhausted, RESOURCE_EXHAUSTED)
DECLARE_ERROR(Unavailable, UNAVAILABLE)
DECLARE_ERROR(FailedPrecondition, FAILED_PRECONDITION)
DECLARE_ERROR(OutOfRange, OUT_OF_RANGE)
DECLARE_ERROR(Unimplemented, UNIMPLEMENTED)
DECLARE_ERROR(Internal, INTERNAL)
DECLARE_ERROR(Aborted, ABORTED)
DECLARE_ERROR(DeadlineExceeded, DEADLINE_EXCEEDED)
DECLARE_ERROR(DataLoss, DATA_LOSS)
DECLARE_ERROR(Unknown, UNKNOWN)
DECLARE_ERROR(PermissionDenied, PERMISSION_DENIED)
DECLARE_ERROR(Unauthenticated, UNAUTHENTICATED)

}  // namespace util

#endif  // THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_CANONICAL_ERRORS_H_
