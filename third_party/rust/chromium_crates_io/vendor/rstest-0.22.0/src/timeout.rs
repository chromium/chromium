use std::{sync::mpsc, thread, time::Duration};

#[cfg(feature = "async-timeout")]
use futures::{select, Future, FutureExt};
#[cfg(feature = "async-timeout")]
use futures_timer::Delay;

pub fn execute_with_timeout_sync<T: 'static + Send, F: FnOnce() -> T + Send + 'static>(
    code: F,
    timeout: Duration,
) -> T {
    let (sender, receiver) = mpsc::channel();
    let thread = if let Some(name) = thread::current().name() {
        thread::Builder::new().name(name.to_string())
    } else {
        thread::Builder::new()
    };
    let handle = thread.spawn(move || sender.send(code())).unwrap();
    match receiver.recv_timeout(timeout) {
        Ok(result) => {
            // Unwraps are safe because we got a result from the thread, which is not a `SendError`,
            // and there was no panic within the thread which caused a disconnect.
            handle.join().unwrap().unwrap();
            result
        }
        Err(mpsc::RecvTimeoutError::Timeout) => panic!("Timeout {:?} expired", timeout),
        Err(mpsc::RecvTimeoutError::Disconnected) => match handle.join() {
            Err(any) => std::panic::resume_unwind(any),
            Ok(_) => unreachable!(),
        },
    }
}

#[cfg(feature = "async-timeout")]
pub async fn execute_with_timeout_async<T, Fut: Future<Output = T>, F: FnOnce() -> Fut>(
    code: F,
    timeout: Duration,
) -> T {
    select! {
        () = async {
            Delay::new(timeout).await;
        }.fuse() => panic!("Timeout {:?} expired", timeout),
        out = code().fuse() => out,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[cfg(feature = "async-timeout")]
    mod async_version {

        use super::*;
        use std::time::Duration;

        async fn delayed_sum(a: u32, b: u32, delay: Duration) -> u32 {
            async_std::task::sleep(delay).await;
            a + b
        }

        async fn test(delay: Duration) {
            let result = delayed_sum(2, 2, delay).await;
            assert_eq!(result, 4);
        }

        mod use_async_std_runtime {
            use super::*;

            #[async_std::test]
            #[should_panic]
            async fn should_fail() {
                execute_with_timeout_async(
                    || test(Duration::from_millis(40)),
                    Duration::from_millis(10),
                )
                .await
            }

            #[async_std::test]
            async fn should_pass() {
                execute_with_timeout_async(
                    || test(Duration::from_millis(10)),
                    Duration::from_millis(40),
                )
                .await
            }

            #[async_std::test]
            #[should_panic = "inner message"]
            async fn should_fail_for_panic_with_right_panic_message() {
                execute_with_timeout_async(
                    || async {
                        panic!("inner message");
                    },
                    Duration::from_millis(30),
                )
                .await
            }

            #[async_std::test]
            async fn should_compile_also_with_no_copy_move() {
                struct S {}
                async fn test(_s: S) {
                    assert!(true);
                }
                let s = S {};

                execute_with_timeout_async(move || test(s), Duration::from_millis(20)).await
            }
        }

        mod use_tokio_runtime {
            use super::*;

            #[tokio::test]
            #[should_panic]
            async fn should_fail() {
                execute_with_timeout_async(
                    || test(Duration::from_millis(40)),
                    Duration::from_millis(10),
                )
                .await
            }

            #[async_std::test]
            #[should_panic = "inner message"]
            async fn should_fail_for_panic_with_right_panic_message() {
                execute_with_timeout_async(
                    || async {
                        panic!("inner message");
                    },
                    Duration::from_millis(30),
                )
                .await
            }

            #[tokio::test]
            async fn should_pass() {
                execute_with_timeout_async(
                    || test(Duration::from_millis(10)),
                    Duration::from_millis(40),
                )
                .await
            }
        }
    }

    mod thread_version {
        use super::*;

        pub fn delayed_sum(a: u32, b: u32, delay: Duration) -> u32 {
            std::thread::sleep(delay);
            a + b
        }

        fn test(delay: Duration) {
            let result = delayed_sum(2, 2, delay);
            assert_eq!(result, 4);
        }

        #[test]
        fn should_pass() {
            execute_with_timeout_sync(
                || test(Duration::from_millis(30)),
                Duration::from_millis(70),
            )
        }

        #[test]
        #[should_panic = "inner message"]
        fn should_fail_for_panic_with_right_panic_message() {
            execute_with_timeout_sync(
                || {
                    panic!("inner message");
                },
                Duration::from_millis(100),
            )
        }

        #[test]
        #[should_panic]
        fn should_fail() {
            execute_with_timeout_sync(
                || test(Duration::from_millis(70)),
                Duration::from_millis(30),
            )
        }
        #[test]
        fn should_compile_also_with_no_copy_move() {
            struct S {}
            fn test(_s: S) {
                assert!(true);
            }
            let s = S {};

            execute_with_timeout_sync(move || test(s), Duration::from_millis(20))
        }
    }
}
