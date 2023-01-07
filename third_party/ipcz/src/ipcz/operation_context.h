// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_OPERATION_CONTEXT_H_
#define IPCZ_SRC_IPCZ_OPERATION_CONTEXT_H_

namespace ipcz {

// Structure to capture any relevant context regarding an ongoing ipcz
// operation. This is plumbed throughout methods on Router and other related
// objects as needed to provide context for any events emitted by ipcz.
struct OperationContext {
  // Indicates the nature of the innermost entry point into ipcz for the current
  // call stack. For any call stack within ipcz which propagates an
  // OperationContext, the correct EntryPoint can be deduced by walking up the
  // stack until hitting either an API entry point (i.e. an explicit IpczAPI
  // function invocation) OR a driver transport notification (i.e. an
  // IpczTransportActivityHandler invocation.)
  enum class EntryPoint {
    // The current innermost stack frame entering ipcz is a direct IpczAPI call.
    kAPICall,

    // The current innermost stack frame entering ipcz is a driver transport
    // activity notification.
    kTransportNotification,
  };

  static constexpr EntryPoint kAPICall = EntryPoint::kAPICall;
  static constexpr EntryPoint kTransportNotification =
      EntryPoint::kTransportNotification;

  explicit OperationContext(EntryPoint entry_point)
      : entry_point_(entry_point) {}
  OperationContext(const OperationContext&) = default;
  OperationContext& operator=(const OperationContext&) = default;

  bool is_api_call() const { return entry_point_ == kAPICall; }

 private:
  EntryPoint entry_point_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_OPERATION_CONTEXT_H_
