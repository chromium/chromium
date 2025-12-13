#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"

#include <string>

#include "third_party/webrtc/p2p/base/connection.h"
#include "third_party/webrtc/rtc_base/strings/string_builder.h"

namespace blink {

namespace {

IceConnection::WriteState ConvertFromWebrtcWriteState(
    webrtc::Connection::WriteState write_state) {
  switch (write_state) {
    case webrtc::Connection::WriteState::STATE_WRITE_INIT:
      return IceConnection::WriteState::STATE_WRITE_INIT;
    case webrtc::Connection::STATE_WRITABLE:
      return IceConnection::WriteState::STATE_WRITABLE;
    case webrtc::Connection::STATE_WRITE_UNRELIABLE:
      return IceConnection::WriteState::STATE_WRITE_UNRELIABLE;
    case webrtc::Connection::STATE_WRITE_TIMEOUT:
      return IceConnection::WriteState::STATE_WRITE_TIMEOUT;
  }
}

}  // unnamed namespace

IceConnection::IceConnection(const webrtc::Connection* connection)
    : id_(connection->id()),
      local_candidate_(connection->local_candidate()),
      remote_candidate_(connection->remote_candidate()),
      connected_(connection->connected()),
      selected_(connection->selected()),
      write_state_(ConvertFromWebrtcWriteState(connection->write_state())),
      last_ping_sent_(connection->LastPingSent()),
      last_ping_received_(connection->LastPingReceived()),
      last_data_received_(connection->LastDataReceived()),
      last_ping_response_received_(connection->LastPingResponseReceived()),
      num_pings_sent_(connection->num_pings_sent()) {}
// TODO(crbug.com/1369096): rtt_samples_: extract RTT samples from connection.

std::string IceConnection::ToString() const {
  webrtc::StringBuilder ss;
  ss << "IceConn[" << id_ << ":" << local_candidate_.ToString() << ":"
     << remote_candidate_.ToString() << "]";
  return ss.Release();
}

}  // namespace blink
