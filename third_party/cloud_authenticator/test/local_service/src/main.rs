// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! `local_service` exposes the `processor` crate over a local, unencrypted
//! WebSocket connection. This allows the enclave service to be simulated on the
//! local machine. It keeps state in two files in the working directory:
//! "state.transparent" and "state.confidential".

extern crate alloc;
extern crate base64;
extern crate cbor;
extern crate crypto;
extern crate handshake;
extern crate hex;
extern crate processor;

use cbor::{cbor, Value};
use crypto::P256Scalar;
use processor::{ClientState, StateUpdate};
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};

// Frame types from https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
const BINARY: u8 = 2;
const CONTINUATION: u8 = 2;
const WEBSOCKET_PROTOCOL: &str = "cloudauthenticator";

/// Completly fills `buf` with data from `conn` and returns true iff successful.
fn read_all(mut conn: &TcpStream, buf: &mut [u8]) -> bool {
    let mut done = 0;
    while done < buf.len() {
        let Ok(bytes_read) = conn.read(&mut buf[done..]) else {
            return false;
        };
        if bytes_read == 0 {
            return false;
        }
        done += bytes_read;
    }
    true
}

/// Write the full contents of `buf` to `conn`. Returns true iff successful.
fn write_all(mut conn: &TcpStream, buf: &[u8]) -> bool {
    let mut done = 0;
    while done < buf.len() {
        let Ok(bytes_written) = conn.write(&buf[done..]) else {
            return false;
        };
        done += bytes_written;
    }
    true
}

/// `next_line` recognises TLS handshakes and returns them as a special error.
enum NextLineError {
    /// The client probably sent a TLS handshake, not an HTTP request.
    TlsHandshake,
    /// Some other I/O or UTF-8 error.
    OtherError,
}

impl<E> From<E> for NextLineError
where
    E: std::error::Error,
{
    fn from(_: E) -> NextLineError {
        Self::OtherError
    }
}

/// Reads a "\r\n"-terminated line from `conn` and returns it without that
/// terminator. (Inefficient, but we don't mind in this context.)
fn next_line(mut conn: &TcpStream) -> Result<String, NextLineError> {
    let mut ret = Vec::with_capacity(32);
    let mut seen_cr = false;
    loop {
        let mut buf = [0u8; 1];
        if conn.read(&mut buf)? == 0 {
            return Err(NextLineError::OtherError);
        }
        if ret.is_empty() && buf[0] == 0x16 {
            return Err(NextLineError::TlsHandshake);
        }
        if seen_cr && buf[0] == b'\n' {
            ret.pop();
            return Ok(String::from_utf8(ret)?);
        }
        seen_cr = buf[0] == b'\r';
        ret.push(buf[0]);
    }
}

/// Reads a WebSocket frame from `conn`, returning whether it's the final frame
/// of a message, the type of the frame, and its contents. See
/// https://datatracker.ietf.org/doc/html/rfc6455#section-5.
fn read_frame(conn: &TcpStream) -> Option<(bool, u8, Vec<u8>)> {
    let mut buf = [0u8; 6];
    if !read_all(conn, &mut buf) {
        return None;
    }

    // See https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
    let fin = buf[0] & 0x80 == 0x80;
    let opcode = buf[0] & 0x0f;
    let has_mask = buf[1] & 0x80 == 0x80;
    if !has_mask {
        eprintln!("frame from client should be masked");
        return None;
    }
    let payload_len = buf[1] & 0x7f;
    let mut mask = [0u8; 4];

    let payload_len = if payload_len == 127 {
        // Lengths must be minimally encoded. So this suggests a frame > 64KiB,
        // which we don't need to handle in this context.
        eprintln!("unsupported 64-bit length");
        return None;
    } else if payload_len == 126 {
        let mut extra = [0u8; 2];
        if !read_all(conn, &mut extra) {
            return None;
        }
        mask[0] = buf[4];
        mask[1] = buf[5];
        mask[2] = extra[0];
        mask[3] = extra[1];
        (buf[2] as usize) << 8 | (buf[3] as usize)
    } else {
        mask.copy_from_slice(&buf[2..]);
        payload_len as usize
    };

    let mut ret = vec![0; payload_len];
    if !read_all(conn, &mut ret) {
        return None;
    }
    for i in 0..ret.len() {
        ret[i] ^= mask[i % 4];
    }
    Some((fin, opcode, ret))
}

/// Reads a WebSocket message from `conn`. See
/// https://datatracker.ietf.org/doc/html/rfc6455#section-6.
fn read_msg(conn: &TcpStream) -> Option<Vec<u8>> {
    let (fin, opcode, payload) = read_frame(conn)?;

    if opcode != BINARY {
        eprintln!("unexpected message type {}", opcode);
        return None;
    }
    if fin {
        return Some(payload);
    }

    let mut ret = payload;
    loop {
        let (fin, opcode, payload) = read_frame(conn)?;
        if opcode != CONTINUATION {
            eprintln!("unexpected message type {}", opcode);
            return None;
        }
        ret.extend_from_slice(&payload);
        if fin {
            return Some(ret);
        }
    }
}

/// Write `msg` as a WebSocket message to `conn`. Returns true iff successful.
fn write_msg(conn: &TcpStream, msg: &[u8]) -> bool {
    // See https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
    let len = msg.len();
    if len < 126 {
        let header = [0x80 | BINARY, len as u8];
        write_all(conn, &header) && write_all(conn, msg)
    } else if len < 0x10000 {
        let header = [0x80 | BINARY, 126, (len >> 8) as u8, len as u8];
        write_all(conn, &header) && write_all(conn, msg)
    } else {
        // Frames larger than 64KiB don't need to be supported in this context.
        false
    }
}

/// Calculate the correct response to a WebSocket challenge. This is checked by
/// the client to ensure that the server intends to negotiate a WebSocket
/// connection. See https://datatracker.ietf.org/doc/html/rfc6455#section-1.3
fn calculate_websocket_accept(key: &[u8]) -> String {
    let digest = crypto::sha1_two_part(key, b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    base64::encode(digest)
}

struct EnclaveServer {
    identity_private_key_bytes: [u8; 32],
}

impl EnclaveServer {
    fn handle_connection(&mut self, conn: TcpStream) {
        eprintln!("Accepted connection from {:?}", conn.peer_addr().unwrap());

        let mut seen_first_line = false;
        let mut websocket_key: Option<String> = None;
        let mut websocket_protocol: Option<String> = None;
        let mut has_reauthentication_header = false;
        loop {
            let line = match next_line(&conn) {
                Ok(line) => line,
                Err(NextLineError::OtherError) => return,
                Err(NextLineError::TlsHandshake) => panic!(
                    "TLS handshake received. This server only speaks plaintext. Ensure that you have specified the address with ws://, not wss://"
                ),
            };
            if line.is_empty() {
                break;
            }
            if !seen_first_line {
                seen_first_line = true;
                continue;
            }

            let Some((key, value)) = line.split_once(':') else {
                eprintln!("bad header line");
                return;
            };
            let key = key.trim();
            match key.to_lowercase().as_str() {
                "sec-websocket-key" => websocket_key = Some(String::from(value.trim())),
                "sec-websocket-protocol" => websocket_protocol = Some(String::from(value.trim())),
                "reauthentication" => has_reauthentication_header = true,
                _ => (),
            }
        }

        let Some(websocket_key) = websocket_key else {
            eprintln!("bad WebSocket request");
            return;
        };

        match websocket_protocol {
            Some(protocol) if protocol.as_str() == WEBSOCKET_PROTOCOL => (),
            _ => {
                eprintln!("missing expected WebSocket protocol");
                return;
            }
        }

        const RESPONSE : &[u8] = b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
        let accept_value = calculate_websocket_accept(websocket_key.as_bytes());
        const PROTOCOL_HEADER: &[u8] = b"Sec-WebSocket-Protocol: ";
        const NEWLINE: &[u8] = b"\r\n";
        if !write_all(&conn, RESPONSE)
            || !write_all(&conn, accept_value.as_bytes())
            || !write_all(&conn, NEWLINE)
            || !write_all(&conn, PROTOCOL_HEADER)
            || !write_all(&conn, WEBSOCKET_PROTOCOL.as_bytes())
            || !write_all(&conn, NEWLINE)
            || !write_all(&conn, NEWLINE)
        {
            return;
        }

        let Some(handshake_request) = read_msg(&conn) else {
            return;
        };
        let mut handshake_response =
            match handshake::respond(&self.identity_private_key_bytes, &handshake_request) {
                Ok(r) => r,
                Err(e) => {
                    eprintln!("Failed to generate handshake response: {:?}", e);
                    return;
                }
            };
        if !write_msg(&conn, &handshake_response.response) {
            return;
        }

        let Some(cmd_request) = read_msg(&conn) else {
            return;
        };
        let Ok(commands) = handshake_response.crypter.decrypt(&cmd_request) else {
            eprintln!("Failed to decrypt commands");
            return;
        };

        const CONFIDENTIAL_PATH: &str = "state.confidential";
        const TRANSPARENT_PATH: &str = "state.transparent";

        let client_state = std::fs::read(CONFIDENTIAL_PATH)
            .and_then(|confidential| {
                std::fs::read(TRANSPARENT_PATH).map(|transparent| {
                    ClientState::Explicit(processor::StateData { transparent, confidential })
                })
            })
            .unwrap_or(ClientState::Initial);

        let cbor_response = match processor::process_client_msg(
            client_state,
            processor::ExternalContext {
                // This timestamp is fixed so that any XML files submitted by tests will be
                // considered unexpired.
                current_time_epoch_millis: 1707344402000,
                client_device_identifier: Vec::new(),
                is_reauthenticated: has_reauthentication_header,
            },
            &handshake_response.handshake_hash,
            commands,
        ) {
            Ok((result_array, state_update)) => {
                let state_data = match state_update {
                    StateUpdate::Major(data) => Some(data),
                    StateUpdate::Minor(data) => Some(data),
                    StateUpdate::None => None,
                };

                if let Some(state_data) = state_data {
                    std::fs::write(CONFIDENTIAL_PATH, &state_data.confidential).unwrap();
                    std::fs::write(TRANSPARENT_PATH, &state_data.transparent).unwrap();
                }

                cbor!({"ok": result_array})
            }
            Err(err) => {
                eprintln!("{:?}", err);

                let err = match err {
                    processor::Error::UnknownClient => Value::Int(0),
                    processor::Error::Str(s) => Value::String(String::from(s)),
                    _ => Value::String(format!("{:?}", err)),
                };
                cbor!({"err": err})
            }
        };

        let cmd_response = handshake_response.crypter.encrypt(&cbor_response.to_bytes()).unwrap();
        write_msg(&conn, &cmd_response);
    }
}

fn main() {
    // The corresponding hex public that has to be manually provided to the
    // test client is:
    // 046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5
    let mut identity_private_key_bytes = [0u8; 32];
    identity_private_key_bytes[31] = 1;
    let mut service = EnclaveServer { identity_private_key_bytes };

    let scalar: P256Scalar = (&identity_private_key_bytes).try_into().unwrap();
    eprintln!("Public key is {}", hex::encode(scalar.compute_public_key()));

    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let local_addr = listener.local_addr().unwrap();
    println!("{}", local_addr.port());
    eprintln!("Listening on ws://{}", local_addr);
    for stream in listener.incoming() {
        let stream = stream.unwrap();
        stream.set_nodelay(true).unwrap();
        service.handle_connection(stream);
    }
}
