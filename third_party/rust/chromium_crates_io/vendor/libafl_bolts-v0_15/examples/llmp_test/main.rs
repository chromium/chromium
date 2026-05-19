/*!
This shows how llmp can be used directly, without libafl abstractions
*/
extern crate alloc;

use core::marker::PhantomData;
#[cfg(all(feature = "std", not(target_os = "haiku")))]
use core::num::NonZeroUsize;
#[cfg(not(target_os = "haiku"))]
use core::time::Duration;
#[cfg(all(feature = "std", not(target_os = "haiku")))]
use std::{thread, time};

use libafl_bolts::llmp::{LlmpBrokerInner, LlmpMsgHookResult};
#[cfg(all(feature = "std", not(target_os = "haiku")))]
use libafl_bolts::{
    ClientId, Error, SimpleStderrLogger,
    llmp::{self, Flags, LlmpHook, Tag},
    shmem::{ShMemProvider, StdShMemProvider},
};
use tuple_list::tuple_list;

#[cfg(all(feature = "std", not(target_os = "haiku")))]
const _TAG_SIMPLE_U32_V1: Tag = Tag(0x5130_0321);
#[cfg(all(feature = "std", not(target_os = "haiku")))]
const _TAG_MATH_RESULT_V1: Tag = Tag(0x7747_4331);
#[cfg(all(feature = "std", not(target_os = "haiku")))]
const _TAG_1MEG_V1: Tag = Tag(0xB111_1161);

/// The time the broker will wait for things to happen before printing a message
#[cfg(all(feature = "std", not(target_os = "haiku")))]
const BROKER_TIMEOUT: Duration = Duration::from_secs(10);

/// How long the broker may sleep between forwarding a new chunk of sent messages
#[cfg(all(feature = "std", not(target_os = "haiku")))]
const SLEEP_BETWEEN_FORWARDS: Duration = Duration::from_millis(5);

#[cfg(all(feature = "std", not(target_os = "haiku")))]
static LOGGER: SimpleStderrLogger = SimpleStderrLogger::new();

#[cfg(all(feature = "std", not(target_os = "haiku")))]
fn adder_loop(port: u16) -> Result<(), Box<dyn core::error::Error>> {
    let shmem_provider = StdShMemProvider::new()?;
    let mut client = llmp::LlmpClient::create_attach_to_tcp(shmem_provider, port)?;
    let mut last_result: u32 = 0;
    let mut current_result: u32 = 0;
    loop {
        let mut msg_counter = 0;
        loop {
            let Some((sender, tag, buf)) = client.recv_buf()? else {
                break;
            };
            msg_counter += 1;
            match tag {
                _TAG_SIMPLE_U32_V1 => {
                    current_result =
                        current_result.wrapping_add(u32::from_le_bytes(buf.try_into()?));
                }
                _ => println!(
                    "Adder Client ignored unknown message {:?} from client {:?} with {} bytes",
                    tag,
                    sender,
                    buf.len()
                ),
            }
        }

        if current_result != last_result {
            println!("Adder handled {msg_counter} messages, reporting {current_result} to broker");

            client.send_buf(_TAG_MATH_RESULT_V1, &current_result.to_le_bytes())?;
            last_result = current_result;
        }

        thread::sleep(time::Duration::from_millis(100));
    }
}

#[cfg(all(feature = "std", not(target_os = "haiku")))]
fn large_msg_loop(port: u16) -> Result<(), Box<dyn core::error::Error>> {
    let mut client = llmp::LlmpClient::create_attach_to_tcp(StdShMemProvider::new()?, port)?;

    #[cfg(not(target_vendor = "apple"))]
    let meg_buf = vec![1u8; 1 << 20];
    #[cfg(target_vendor = "apple")]
    let meg_buf = vec![1u8; 1 << 19];

    loop {
        client.send_buf(_TAG_1MEG_V1, &meg_buf)?;
        #[cfg(not(target_vendor = "apple"))]
        println!("Sending the next megabyte");
        #[cfg(target_vendor = "apple")]
        println!("Sending the next half megabyte (Apple had issues with >1 meg)");
        thread::sleep(time::Duration::from_millis(100));
    }
}

pub struct LlmpExampleHook<SP> {
    phantom: PhantomData<SP>,
}

impl<SP> LlmpExampleHook<SP> {
    #[must_use]
    pub fn new() -> Self {
        Self {
            phantom: PhantomData,
        }
    }
}

impl<SP> Default for LlmpExampleHook<SP> {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(all(feature = "std", not(target_os = "haiku")))]
impl<SHM, SP> LlmpHook<SHM, SP> for LlmpExampleHook<SP>
where
    SP: ShMemProvider<ShMem = SHM> + 'static,
{
    fn on_new_message(
        &mut self,
        _broker_inner: &mut LlmpBrokerInner<SHM, SP>,
        client_id: ClientId,
        msg_tag: &mut Tag,
        _msg_flags: &mut Flags,
        msg: &mut [u8],
        _new_msgs: &mut Vec<(Tag, Flags, Vec<u8>)>,
    ) -> Result<LlmpMsgHookResult, Error> {
        match *msg_tag {
            _TAG_SIMPLE_U32_V1 => {
                println!(
                    "Client {:?} sent message: {:?}",
                    client_id,
                    u32::from_le_bytes(msg.try_into()?)
                );
                Ok(LlmpMsgHookResult::ForwardToClients)
            }
            _TAG_MATH_RESULT_V1 => {
                println!(
                    "Adder Client has this current result: {:?}",
                    u32::from_le_bytes(msg.try_into()?)
                );
                Ok(LlmpMsgHookResult::Handled)
            }
            _ => {
                println!("Unknown message id received: {msg_tag:?}");
                Ok(LlmpMsgHookResult::ForwardToClients)
            }
        }
    }

    fn on_timeout(&mut self) -> Result<(), Error> {
        println!(
            "No client did anything for {} seconds..",
            BROKER_TIMEOUT.as_secs()
        );

        Ok(())
    }
}

#[cfg(target_os = "haiku")]
fn main() {
    eprintln!("LLMP example is currently not supported on no_std. Implement ShMem for no_std.");
}

#[cfg(not(target_os = "haiku"))]
fn main() -> Result<(), Box<dyn core::error::Error>> {
    /* The main node has a broker, and a few worker threads */

    use libafl_bolts::llmp::Broker;

    let mode = std::env::args()
        .nth(1)
        .expect("no mode specified, chose 'broker', 'b2b', 'ctr', 'adder', 'large', or 'exiting'");
    let port: u16 = std::env::args()
        .nth(2)
        .unwrap_or_else(|| "1337".into())
        .parse::<u16>()?;
    // in the b2b use-case, this is our "own" port, we connect to the "normal" broker node on startup.
    let b2b_port: u16 = std::env::args()
        .nth(3)
        .unwrap_or_else(|| "4242".into())
        .parse::<u16>()?;

    log::set_logger(&LOGGER).unwrap();
    log::set_max_level(log::LevelFilter::Trace);
    println!("Launching in mode {mode} on port {port}");

    match mode.as_str() {
        "broker" => {
            let mut broker = llmp::LlmpBroker::new(
                StdShMemProvider::new()?,
                tuple_list!(LlmpExampleHook::new()),
            )?;
            broker.inner_mut().launch_tcp_listener_on(port)?;
            // Exit when we got at least _n_ nodes, and all of them quit.
            broker.set_exit_after(NonZeroUsize::new(1_usize).unwrap());
            broker.loop_with_timeouts(BROKER_TIMEOUT, Some(SLEEP_BETWEEN_FORWARDS));
        }
        "b2b" => {
            let mut broker = llmp::LlmpBroker::new(
                StdShMemProvider::new()?,
                tuple_list!(LlmpExampleHook::new()),
            )?;
            broker.inner_mut().launch_tcp_listener_on(b2b_port)?;
            // connect back to the main broker.
            broker.inner_mut().connect_b2b(("127.0.0.1", port))?;
            broker.loop_with_timeouts(BROKER_TIMEOUT, Some(SLEEP_BETWEEN_FORWARDS));
        }
        "ctr" => {
            let mut client =
                llmp::LlmpClient::create_attach_to_tcp(StdShMemProvider::new()?, port)?;
            let mut counter: u32 = 0;
            loop {
                counter = counter.wrapping_add(1);
                client.send_buf(_TAG_SIMPLE_U32_V1, &counter.to_le_bytes())?;
                println!("CTR Client writing {counter}");
                thread::sleep(Duration::from_secs(1));
            }
        }
        "adder" => {
            adder_loop(port)?;
        }
        "large" => {
            large_msg_loop(port)?;
        }
        "exiting" => {
            let mut client =
                llmp::LlmpClient::create_attach_to_tcp(StdShMemProvider::new()?, port)?;
            for i in 0..10_u32 {
                client.send_buf(_TAG_SIMPLE_U32_V1, &i.to_le_bytes())?;
                println!("Exiting Client writing {i}");
                thread::sleep(Duration::from_millis(10));
            }
            log::info!("Exiting Client exits");
            client.sender_mut().send_exiting()?;

            // there is another way to tell that this client wants to exit.
            // one is to call client.sender_mut().send_exiting()?;
            // you can disconnet the client in this way as long as this client in an unrecoverable state (like in a crash handler)
            // another way to do this is through the detach_from_broker() call
            // you can call detach_from_broker(port); to notify the broker that this broker wants to exit
            // This one is usually for the event restarter to cut off the connection when the client has crashed.
            // In that case we don't have access to the llmp client of the client anymore, but we can use detach_from_broker instead
        }
        _ => {
            println!("No valid mode supplied");
        }
    }
    Ok(())
}
