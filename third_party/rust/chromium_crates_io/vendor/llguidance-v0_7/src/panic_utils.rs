use std::backtrace::Backtrace;
use std::cell::Cell;
use std::sync::Once;
use std::{any::Any, panic::UnwindSafe};

use anyhow::Result;

thread_local! {
    static UNWIND_COUNT: Cell<usize> = const { Cell::new(0) };
    static BACKTRACE: Cell<Option<Backtrace>> = const { Cell::new(None) };
}

static INSTALL_HOOK: Once = Once::new();

pub fn mk_panic_error(info: &Box<dyn Any + Send>) -> String {
    let msg = match info.downcast_ref::<&'static str>() {
        Some(s) => *s,
        None => match info.downcast_ref::<String>() {
            Some(s) => &s[..],
            None => "non-string panic!()",
        },
    };

    let b = BACKTRACE.with(|b| b.take());

    if let Some(b) = b {
        format!("panic: {msg}\n{b}")
    } else {
        format!("panic: {msg}")
    }
}

pub fn catch_unwind<F: FnOnce() -> Result<R> + UnwindSafe, R>(f: F) -> Result<R> {
    INSTALL_HOOK.call_once(|| {
        let prev = std::panic::take_hook();
        std::panic::set_hook(Box::new(move |info| {
            if UNWIND_COUNT.with(|count| count.get()) == 0 {
                prev(info)
            } else {
                let trace = Backtrace::force_capture();
                BACKTRACE.with(move |b| b.set(Some(trace)));
            }
        }));
    });

    BACKTRACE.with(|b| b.set(None));
    UNWIND_COUNT.with(|count| count.set(count.get() + 1));

    let r = match std::panic::catch_unwind(f) {
        Ok(r) => r,
        Err(e) => Err(anyhow::anyhow!(mk_panic_error(&e))),
    };

    UNWIND_COUNT.with(|count| count.set(count.get() - 1));

    r
}
