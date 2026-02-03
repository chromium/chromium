/// The primary separator of path components for windows platforms
pub const SEPARATOR: char = '\\';

/// The primary separator of path components for windows platforms
pub const SEPARATOR_STR: &str = "\\";

/// The alternate separator of path components for windows platforms
pub const ALT_SEPARATOR: char = '/';

/// The alternate separator of path components for windows platforms
pub const ALT_SEPARATOR_STR: &str = "/";

/// Path component value that represents the parent directory
pub const PARENT_DIR: &[u8] = b"..";

/// Path component value that represents the parent directory
pub const PARENT_DIR_STR: &str = "..";

/// Path component value that represents the current directory
pub const CURRENT_DIR: &[u8] = b".";

/// Path component value that represents the current directory
pub const CURRENT_DIR_STR: &str = ".";

/// Reserved names (case insensitive) that cannot be used with files or directories
/// for personal use (system only)
pub const RESERVED_DEVICE_NAMES: &[&[u8]] = &[
    b"CON", b"PRN", b"AUX", b"NUL", b"COM1", b"COM2", b"COM3", b"COM4", b"COM5", b"COM6", b"COM7",
    b"COM8", b"COM9", b"COM0", b"LPT1", b"LPT2", b"LPT3", b"LPT4", b"LPT5", b"LPT6", b"LPT7",
    b"LPT8", b"LPT9", b"LPT0",
];

/// Reserved names (case insensitive) that cannot be used with files or directories
/// for personal use (system only)
pub const RESERVED_DEVICE_NAMES_STR: &[&str] = &[
    "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8",
    "COM9", "COM0", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", "LPT0",
];

/// Bytes that are not allowed in file or directory names
#[allow(clippy::byte_char_slices)]
pub const DISALLOWED_FILENAME_BYTES: &[u8] =
    &[b'\\', b'/', b':', b'?', b'*', b'"', b'>', b'<', b'|', b'\0'];

pub const DISALLOWED_FILENAME_CHARS: &[char] =
    &['\\', '/', ':', '?', '*', '"', '>', '<', '|', '\0'];
