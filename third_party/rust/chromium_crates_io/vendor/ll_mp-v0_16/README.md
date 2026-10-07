# `LL_MP`: Low Level Message Passing for fast IPC

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

This crate provides a low-level message passing (LLMP) mechanism designed for high-performance inter-process communication (IPC), particularly useful in fuzzing scenarios. It leverages shared memory to achieve lock-free communication between processes, minimizing overhead and maximizing throughput.

## How it works

To send new messages, the clients place a new message at the end of their
`client_out_mem`. If the current map is filled up, they place an end of page (`EOP`)
msg and alloc a new [`ShMem`].
Once the broker mapped this same page, it flags it as safe for unmapping.

```text
[client0]        [client1]    ...    [clientN]
  |                  |                 /
[client0_out] [client1_out] ... [clientN_out]
  |                 /                /
  |________________/                /
  |________________________________/
 \|/
[broker]
```

After the broker received a new message for clientN, (`clientN_out->current_id
!= last_message->message_id`) the broker will copy the message content to its
own, centralized page.

The clients periodically check (`current_broadcast_shmem->current_id !=
last_message->message_id`) for new incoming messages. If the page is filled up,
the broker instead creates a new page and places an end of page (`EOP`)
message in its queue. The `EOP` buf contains the new description to
access the shared map. The clients then switch over to read from that new
current map.

```text
[broker]
  |
[current_broadcast_shmem]
  |
  |___________________________________
  |_________________                  \
  |                 \                  \
  |                  |                  |
 \|/                \|/                \|/
[client0]        [client1]    ...    [clientN]
```

In the future, if we would need zero copy, the `current_broadcast_shmem` could instead
list the `client_out_shmem` ID an offset for each message. In that case, the clients
also need to create a new [`ShMem`] each time their bufs are filled up.


## Usage Example

Here is a simple example of a broker and a client communicating in the same process using threads. In a real-world scenario, the client and broker would run in separate processes.

This example requires the `llmp_serde` feature to be enabled, which allows sending and receiving `serde`-serializable structs.

```rust,ignore
use libafl_bolts::llmp;
use serde::{Serialize, Deserialize};
use std::{thread, time::Duration};

const BROKER_PORT: u16 = 1337;

// A simple message type that we can serialize
#[derive(Serialize, Deserialize, Debug)]
struct MyMessage {
    data: String,
}

// The client part
fn client_logic() {
    // Give the broker a moment to start
    thread::sleep(Duration::from_millis(100));

    // Connect to the broker over TCP
    let mut client = llmp::LlmpClient::new(llmp::LlmpConnection::Tcp { port: BROKER_PORT }, 0).expect("Failed to connect to broker");

    // Create a message
    let msg = MyMessage {
        data: "Hello from client!".to_string(),
    };

    // Send the message to the broker
    client.send_obj(&msg).expect("Failed to send message");
    println!("Client sent a message.");
}

// The broker part
fn main() {
    // Create a new broker listening on a TCP port
    let mut broker = llmp::LlmpBroker::new(llmp::LlmpConnection::Tcp { port: BROKER_PORT }).expect("Failed to start broker");

    // Spawn a client thread
    let client_handle = thread::spawn(client_logic);

    println!("Broker started, waiting for message...");

    // Block until we receive a message
    let (client_id, msg) = loop {
        // Handle events, this is non-blocking
        broker.loop_once().unwrap();
        
        // Try to receive a message.
        // This is also non-blocking and will return Ok(None) if no message is available.
        if let Ok(Some(msg)) = broker.recv_obj::<MyMessage>() {
            break msg;
        }

        // Don't spin the CPU
        thread::sleep(Duration::from_millis(10));
    };

    println!("Broker received: '{:?}' from client {:?}", msg, client_id);

    // Clean up
    client_handle.join().unwrap();
}
```

## Fancy features

* **Shared Memory IPC**: Utilizes shared memory segments for efficient data exchange between processes.
* **Lock-Free Design**: Employs atomic operations and careful memory management to avoid locks, reducing contention and improving performance.
* **Message-Based Communication**: Provides a clear message-passing interface for sending and receiving data.
* **Scalability**: Designed to scale across multiple processes and potentially multiple machines (when combined with other networking layers).
* **Fuzzing-Oriented**: Optimized for the specific needs of fuzzing, such as rapid test case delivery and feedback collection.

## LLMP != LLVM != LLM

It is _not_ related to LLMs, nor to LLVM.
Although it is probably more related to LLVM than LLMs.

## The `LibAFL` Project

The `LibAFL` project is part of [`AFLplusplus`](https://github.com/AFLplusplus) and maintained by

* [Andrea Fioraldi](https://twitter.com/andreafioraldi) <andrea@aflplus.plus>
* [Dominik Maier](https://bsky.app/profile/dmnk.bsky.social) <dominik@aflplus.plus>
* [s1341](https://twitter.com/srubenst1341) <github@shmarya.net>
* [Dongjia Zhang](https://github.com/tokatoka) <toka@aflplus.plus>
* [Addison Crump](https://github.com/addisoncrump) <me@addisoncrump.info>

## Contributing

For bugs, feel free to open issues or contact us directly. Thank you for your support. <3

Even though we will gladly assist you in finishing up your PR, try to

* keep all the crates compiling with *stable* rust (hide the eventual non-stable code under `cfg`s.)
* run `cargo nightly fmt` on your code before pushing
* check the output of `cargo clippy --all` or `./clippy.sh`
* run `cargo build --no-default-features` to check for `no_std` compatibility (and possibly add `#[cfg(feature = "std")]`) to hide parts of your code.

Some parts in this list may sound hard, but don't be afraid to open a PR if you cannot fix them by yourself. We will gladly assist.

#### License

<sup>
Licensed under either of <a href="../LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="../LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>

<br>

<sub>
Dependencies under more restrictive licenses, such as GPL or AGPL, can be enabled
using the respective feature in each crate when it is present, such as the
'agpl' feature of the libafl crate.
</sub>
