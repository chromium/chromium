#include "src/deps/status.h"

#include <string>

namespace util {

Status::Status() {}
Status::~Status() {}

struct Status::Rep {
  error::Code code;
  std::string error_message;
};

Status::Status(error::Code code, const char* error_message) : rep_(new Rep) {
  rep_->code = code;
  rep_->error_message = error_message;
}

Status::Status(error::Code code, const std::string& error_message)
    : rep_(new Rep) {
  rep_->code = code;
  rep_->error_message = error_message;
}

Status::Status(const Status& s)
    : rep_((s.rep_ == nullptr) ? nullptr : new Rep(*s.rep_)) {}

Status& Status::operator=(const Status& s) {
  if (rep_ != s.rep_) {
    rep_.reset((s.rep_ == nullptr) ? nullptr : new Rep(*s.rep_));
  }
  return *this;
}

bool Status::operator==(const Status& s) const { return (rep_ == s.rep_); }

bool Status::operator!=(const Status& s) const { return (rep_ != s.rep_); }

const char* Status::message() const {
  return ok() ? "" : rep_->error_message.c_str();
}

void Status::set_error_message(const char* str) {
  if (rep_ == nullptr) rep_.reset(new Rep);
  rep_->error_message = str;
}

error::Code Status::code() const { return ok() ? error::OK : rep_->code; }

std::string Status::ToString() const {
  if (rep_ == nullptr) return "OK";

  std::string result;
  switch (code()) {
    case error::CANCELLED:
      result = "Cancelled";
      break;
    case error::UNKNOWN:
      result = "Unknown";
      break;
    case error::INVALID_ARGUMENT:
      result = "Invalid argument";
      break;
    case error::DEADLINE_EXCEEDED:
      result = "Deadline exceeded";
      break;
    case error::NOT_FOUND:
      result = "Not found";
      break;
    case error::ALREADY_EXISTS:
      result = "Already exists";
      break;
    case error::PERMISSION_DENIED:
      result = "Permission denied";
      break;
    case error::UNAUTHENTICATED:
      result = "Unauthenticated";
      break;
    case error::RESOURCE_EXHAUSTED:
      result = "Resource exhausted";
      break;
    case error::FAILED_PRECONDITION:
      result = "Failed precondition";
      break;
    case error::ABORTED:
      result = "Aborted";
      break;
    case error::OUT_OF_RANGE:
      result = "Out of range";
      break;
    case error::UNIMPLEMENTED:
      result = "Unimplemented";
      break;
    case error::INTERNAL:
      result = "Internal";
      break;
    case error::UNAVAILABLE:
      result = "Unavailable";
      break;
    case error::DATA_LOSS:
      result = "Data loss";
      break;
    default:
      result = "Unknown code:";
      break;
  }

  result += ": ";
  result += rep_->error_message;
  return result;
}

void Status::IgnoreError() {}

}  // namespace util
