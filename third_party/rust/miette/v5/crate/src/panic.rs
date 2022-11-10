#![cfg(feature = "fancy")]
use std::fmt::Write;

use backtrace::Backtrace;
use thiserror::Error;

use crate::{self as miette, Context, Diagnostic, Result};

/// Tells miette to render panics using its rendering engine.
pub fn set_panic_hook() {
    std::panic::set_hook(Box::new(move |info| {
        let mut message = "Something went wrong".to_string();
        let payload = info.payload();
        if let Some(msg) = payload.downcast_ref::<&str>() {
            message = msg.to_string();
        }
        if let Some(msg) = payload.downcast_ref::<String>() {
            message = msg.clone();
        }
        let mut report: Result<()> = Err(Panic(message).into());
        if let Some(loc) = info.location() {
            report = report
                .with_context(|| format!("at {}:{}:{}", loc.file(), loc.line(), loc.column()));
        }
        if let Err(err) = report.with_context(|| "Main thread panicked.".to_string()) {
            eprintln!("Error: {:?}", err);
        }
    }));
}

#[derive(Debug, Error, Diagnostic)]
#[error("{0}{}", self.maybe_collect_backtrace())]
#[diagnostic(help("set the `RUST_BACKTRACE=1` environment variable to display a backtrace."))]
struct Panic(String);

impl Panic {
    fn maybe_collect_backtrace(&self) -> String {
        if let Ok(var) = std::env::var("RUST_BACKTRACE") {
            if !var.is_empty() && var != "0" {
                // This is all taken from human-panic: https://github.com/rust-cli/human-panic/blob/master/src/report.rs#L55-L107
                const HEX_WIDTH: usize = std::mem::size_of::<usize>() + 2;
                //Padding for next lines after frame's address
                const NEXT_SYMBOL_PADDING: usize = HEX_WIDTH + 6;
                let mut backtrace = String::new();
                for (idx, frame) in Backtrace::new().frames().iter().skip(26).enumerate() {
                    let ip = frame.ip();
                    let _ = write!(backtrace, "\n{:4}: {:2$?}", idx, ip, HEX_WIDTH);

                    let symbols = frame.symbols();
                    if symbols.is_empty() {
                        let _ = write!(backtrace, " - <unresolved>");
                        continue;
                    }

                    for (idx, symbol) in symbols.iter().enumerate() {
                        //Print symbols from this address,
                        //if there are several addresses
                        //we need to put it on next line
                        if idx != 0 {
                            let _ = write!(backtrace, "\n{:1$}", "", NEXT_SYMBOL_PADDING);
                        }

                        if let Some(name) = symbol.name() {
                            let _ = write!(backtrace, " - {}", name);
                        } else {
                            let _ = write!(backtrace, " - <unknown>");
                        }

                        //See if there is debug information with file name and line
                        if let (Some(file), Some(line)) = (symbol.filename(), symbol.lineno()) {
                            let _ = write!(
                                backtrace,
                                "\n{:3$}at {}:{}",
                                "",
                                file.display(),
                                line,
                                NEXT_SYMBOL_PADDING
                            );
                        }
                    }
                }
                return backtrace;
            }
        }
        "".into()
    }
}
