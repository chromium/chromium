#![doc = include_str!("../README.md")]

pub use atty::Stream;

/// Returns true if the current terminal, detected through various environment
/// variables, is known to support hyperlink rendering.
pub fn supports_hyperlinks() -> bool {
    // Hyperlinks can be forced through this env var.
    if let Ok(arg) = std::env::var("FORCE_HYPERLINK") {
        return arg.trim() != "0";
    }

    if std::env::var("DOMTERM").is_ok() {
        // DomTerm
        return true;
    }

    if let Ok(version) = std::env::var("VTE_VERSION") {
        // VTE-based terminals above v0.50 (Gnome Terminal, Guake, ROXTerm, etc)
        if version.parse().unwrap_or(0) >= 5000 {
            return true;
        }
    }

    if let Ok(program) = std::env::var("TERM_PROGRAM") {
        if matches!(
            &program[..],
            "Hyper" | "iTerm.app" | "terminology" | "WezTerm"
        ) {
            return true;
        }
    }

    if let Ok(term) = std::env::var("TERM") {
        // Kitty
        if matches!(&term[..], "xterm-kitty") {
            return true;
        }
    }

    // Windows Terminal and Konsole
    std::env::var("WT_SESSION").is_ok() || std::env::var("KONSOLE_VERSION").is_ok()
}

/// Returns true if `stream` is a TTY, and the current terminal
/// [supports_hyperlinks].
pub fn on(stream: Stream) -> bool {
    (std::env::var("FORCE_HYPERLINK").is_ok() || atty::is(stream)) && supports_hyperlinks()
}
