use std::env;
use std::panic;
use std::process::Command;
use std::sync::atomic::{AtomicBool, Ordering::SeqCst};
use std::sync::Arc;
use std::thread;

const PANICS: usize = 100;
const THREADS: usize = 8;
const VAR: &str = "__THE_TEST_YOU_ARE_LUKE";

fn main() {
    // These run in docker containers on CI where they can't re-exec the test,
    // so just skip these for CI. No other reason this can't run on those
    // platforms though.
    // Miri does not have support for re-execing a file
    if cfg!(unix)
        && (cfg!(target_arch = "arm")
            || cfg!(target_arch = "aarch64")
            || cfg!(target_arch = "s390x"))
        || cfg!(miri)
    {
        println!("test result: ok");
        return;
    }

    if env::var(VAR).is_err() {
        parent();
    } else {
        child();
    }
}

fn parent() {
    let me = env::current_exe().unwrap();
    let result = Command::new(&me)
        .env("RUST_BACKTRACE", "1")
        .env(VAR, "1")
        .output()
        .unwrap();
    if result.status.success() {
        println!("test result: ok");
        return;
    }
    println!("stdout:\n{}", String::from_utf8_lossy(&result.stdout));
    println!("stderr:\n{}", String::from_utf8_lossy(&result.stderr));
    println!("code: {}", result.status);
    panic!();
}

fn child() {
    let done = Arc::new(AtomicBool::new(false));
    let done2 = done.clone();
    let a = thread::spawn(move || {
        while !done2.load(SeqCst) {
            format!("{:?}", backtrace::Backtrace::new());
        }
    });

    let threads = (0..THREADS)
        .map(|_| {
            thread::spawn(|| {
                for _ in 0..PANICS {
                    assert!(panic::catch_unwind(|| {
                        panic!();
                    })
                    .is_err());
                }
            })
        })
        .collect::<Vec<_>>();
    for thread in threads {
        thread.join().unwrap();
    }

    done.store(true, SeqCst);
    a.join().unwrap();
}
