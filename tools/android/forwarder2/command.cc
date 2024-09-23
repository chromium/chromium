// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/command.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string_view>

#include "base/logging.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "tools/android/forwarder2/socket.h"

namespace {


// Command format:
//      <port>:<type>
//
// Where:
//   <port> is a 5-chars zero-padded ASCII decimal integer
//          matching the target port for the command (e.g.
//          '08080' for port 8080)
//   <type> is a 3-char zero-padded ASCII decimal integer
//          matching a command::Type value (e.g. 002 for
//          ACK).
// The column (:) is used as a separator for easier reading.
const int kPortStringSize = 5;
const int kCommandTypeStringSize = 2;
// Command string size also includes the ':' separator char.
const int kCommandStringSize = kPortStringSize + kCommandTypeStringSize + 1;

const int kNoTimeout = -1;

}  // namespace

namespace forwarder2 {

bool ReadCommand(Socket* socket,
                 int* port_out,
                 command::Type* command_type_out) {
  return ReadCommandWithTimeout(socket, port_out, command_type_out, kNoTimeout);
}

bool ReadCommandWithTimeout(Socket* socket,
                            int* port_out,
                            command::Type* command_type_out,
                            int timeout_secs) {
  char command_buffer[kCommandStringSize + 1];
  // To make logging easier.
  command_buffer[kCommandStringSize] = '\0';

  int bytes_read = socket->ReadNumBytesWithTimeout(
      command_buffer, kCommandStringSize, timeout_secs);
  if (bytes_read != kCommandStringSize) {
    if (bytes_read < 0) {
      PLOG(ERROR) << "Read() error";
    } else if (!bytes_read) {
      LOG(ERROR) << "Read() error, endpoint was unexpectedly closed.";
    } else {
      LOG(ERROR) << "Read() error, not enough data received from the socket.";
    }
    return false;
  }

  std::string_view port_str(command_buffer, kPortStringSize);
  if (!base::StringToInt(port_str, port_out)) {
    LOG(ERROR) << "Could not parse the command port string: "
               << port_str;
    return false;
  }

  std::string_view command_type_str(&command_buffer[kPortStringSize + 1],
                                    kCommandTypeStringSize);
  int command_type;
  if (!base::StringToInt(command_type_str, &command_type)) {
    LOG(ERROR) << "Could not parse the command type string: "
               << command_type_str;
    return false;
  }
  *command_type_out = static_cast<command::Type>(command_type);
  return true;
}

bool SendCommand(command::Type command, int port, Socket* socket) {
  char buffer[kCommandStringSize + 1];
  int len = snprintf(buffer, sizeof(buffer), "%05d:%02d", port, command);
  CHECK_EQ(len, kCommandStringSize);
  // Write the full command minus the leading \0 char.
  return socket->WriteNumBytes(buffer, len) == len;
}

bool ReceivedCommand(command::Type command, Socket* socket) {
  return ReceivedCommandWithTimeout(command, socket, kNoTimeout);
}

bool ReceivedCommandWithTimeout(command::Type command,
                                Socket* socket,
                                int timeout_secs) {
  int port;
  command::Type received_command;
  if (!ReadCommandWithTimeout(socket, &port, &received_command, timeout_secs))
    return false;
  return received_command == command;
}

}  // namespace forwarder
