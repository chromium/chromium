/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
// Modifications Copyright (c) Microsoft.

#pragma once

#include <memory>
#include <ostream>
#include <string>
#ifdef _WIN32
#include <winerror.h>
#endif
namespace onnxruntime {
namespace common {

enum StatusCategory {
  NONE = 0,
  SYSTEM = 1,
  ONNXRUNTIME = 2,
};

/**
   Error code for ONNXRuntime.
*/
enum StatusCode {
  OK = 0,
  FAIL = 1,
  INVALID_ARGUMENT = 2,
  NO_SUCHFILE = 3,
  NO_MODEL = 4,
  ENGINE_ERROR = 5,
  RUNTIME_EXCEPTION = 6,
  INVALID_PROTOBUF = 7,
  MODEL_LOADED = 8,
  NOT_IMPLEMENTED = 9,
  INVALID_GRAPH = 10,
  EP_FAIL = 11
};

constexpr const char* StatusCodeToString(StatusCode status) noexcept {
  switch (status) {
    case StatusCode::OK:
      return "SUCCESS";
    case StatusCode::FAIL:
      return "FAIL";
    case StatusCode::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case StatusCode::NO_SUCHFILE:
      return "NO_SUCHFILE";
    case StatusCode::NO_MODEL:
      return "NO_MODEL";
    case StatusCode::ENGINE_ERROR:
      return "ENGINE_ERROR";
    case StatusCode::RUNTIME_EXCEPTION:
      return "RUNTIME_EXCEPTION";
    case StatusCode::INVALID_PROTOBUF:
      return "INVALID_PROTOBUF";
    case StatusCode::MODEL_LOADED:
      return "MODEL_LOADED";
    case StatusCode::NOT_IMPLEMENTED:
      return "NOT_IMPLEMENTED";
    case StatusCode::INVALID_GRAPH:
      return "INVALID_GRAPH";
    case StatusCode::EP_FAIL:
      return "EP_FAIL";
    default:
      return "GENERAL ERROR";
  }
}

#ifdef _WIN32
constexpr HRESULT StatusCodeToHRESULT(StatusCode status) noexcept {
  switch (status) {
    case StatusCode::OK:
      return S_OK;
    case StatusCode::FAIL:
      return E_FAIL;
    case StatusCode::INVALID_ARGUMENT:
      return E_INVALIDARG;
    case StatusCode::NO_SUCHFILE:
      return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    case StatusCode::NO_MODEL:
      return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    case StatusCode::ENGINE_ERROR:
      return E_FAIL;
    case StatusCode::RUNTIME_EXCEPTION:
      return E_FAIL;
    case StatusCode::INVALID_PROTOBUF:
      return HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT);
    case StatusCode::MODEL_LOADED:
      return HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR);
    case StatusCode::NOT_IMPLEMENTED:
      return E_NOTIMPL;
    case StatusCode::INVALID_GRAPH:
      return HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT);
    case StatusCode::EP_FAIL:
      return HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR);
    default:
      return E_FAIL;
  }
}
#endif

class [[nodiscard]] Status {
 public:
  Status() noexcept = default;

  Status(StatusCategory category, int code, const std::string& msg);

  Status(StatusCategory category, int code, const char* msg);

  Status(StatusCategory category, int code);

  Status(const Status& other)
      : state_((other.state_ == nullptr) ? nullptr : new State(*other.state_)) {}
  Status& operator=(const Status& other) {
    if (state_ != other.state_) {
      if (other.state_ == nullptr) {
        state_.reset();
      } else {
        state_.reset(new State(*other.state_));
      }
    }
    return *this;
  }

  Status(Status&&) = default;
  Status& operator=(Status&&) = default;
  ~Status() = default;

  bool IsOK() const {
    return (state_ == nullptr);
  }

  int Code() const noexcept;

  StatusCategory Category() const noexcept;

  const std::string& ErrorMessage() const noexcept;

  std::string ToString() const;

  bool operator==(const Status& other) const {
    return (this->state_ == other.state_) || (ToString() == other.ToString());
  }

  bool operator!=(const Status& other) const {
    return !(*this == other);
  }

  static Status OK() {
    return Status();
  }

 private:
  static const std::string& EmptyString() noexcept;

  struct State {
    State(StatusCategory cat0, int code0, const std::string& msg0)
        : category(cat0), code(code0), msg(msg0) {}

    State(StatusCategory cat0, int code0, const char* msg0)
        : category(cat0), code(code0), msg(msg0) {}

    const StatusCategory category;
    const int code;
    const std::string msg;
  };

  // As long as Code() is OK, state_ == nullptr.
  std::unique_ptr<State> state_;
};

inline std::ostream& operator<<(std::ostream& out, const Status& status) {
  return out << status.ToString();
}

}  // namespace common

// make Status directly available in the onnxruntime namespace as it is widely used
using common::Status;

}  // namespace onnxruntime
