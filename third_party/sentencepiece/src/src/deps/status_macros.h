#ifndef THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_MACROS_H_
#define THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_MACROS_H_

#define RETURN_IF_ERROR(expr)          \
  do {                                 \
    const auto _status = expr;         \
    if (!_status.ok()) return _status; \
  } while (0)

#endif  // THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_STATUS_MACROS_H_
