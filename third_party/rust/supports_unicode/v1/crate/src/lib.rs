#![doc = include_str!("../README.md")]

pub use atty::Stream;

pub fn on(stream: Stream) -> bool {
    if !atty::is(stream) {
        // If we're just piping out, it's fine to spit out unicode! :)
        true
    } else if std::env::consts::OS == "windows" {
        // Just a handful of things!
        std::env::var("CI").is_ok()
        || std::env::var("WT_SESSION").is_ok() // Windows Terminal
        || std::env::var("ConEmuTask") == Ok("{cmd:Cmder}".into()) // ConEmu and cmder
        || std::env::var("TERM_PROGRAM") == Ok("vscode".into())
        || std::env::var("TERM") == Ok("xterm-256color".into())
        || std::env::var("TERM") == Ok("alacritty".into())
    } else if std::env::var("TERM") == Ok("linux".into()) {
        // Linux kernel console. Maybe redundant with the below?...
        false
    } else {
        // From https://github.com/iarna/has-unicode/blob/master/index.js
        let ctype = std::env::var("LC_ALL")
            .or_else(|_| std::env::var("LC_CTYPE"))
            .or_else(|_| std::env::var("LANG"))
            .unwrap_or_else(|_| "".into())
            .to_uppercase();
        ctype.ends_with("UTF8") || ctype.ends_with("UTF-8")
    }
}
