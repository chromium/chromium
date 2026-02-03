use crate::{PlatformPathBuf, Utf8PlatformPathBuf};
use std::convert::TryFrom;
use std::{env, io};

/// Returns the current working directory as [`PlatformPathBuf`].
///
/// # Errors
///
/// Returns an [`Err`] if the current working directory value is invalid
/// or unable to parse the directory with the platform encoding.
///
/// Possible cases:
///
/// * Current directory does not exist.
/// * There are insufficient permissions to access the current directory.
/// * The encoding used to parse the current directory failed to parse.
///
/// # Examples
///
/// ```
/// fn main() -> std::io::Result<()> {
///     let path = typed_path::utils::current_dir()?;
///     println!("The current directory is {}", path.display());
///     Ok(())
/// }
/// ```
pub fn current_dir() -> io::Result<PlatformPathBuf> {
    let std_path = env::current_dir()?;
    match PlatformPathBuf::try_from(std_path) {
        Ok(path) => Ok(path),
        _ => Err(io::Error::new(io::ErrorKind::InvalidData, "wrong encoding")),
    }
}

/// Returns the current working directory as [`Utf8PlatformPathBuf`].
///
/// # Errors
///
/// Returns an [`Err`] if the current working directory value is invalid
/// or unable to parse the directory with the platform encoding.
///
/// Possible cases:
///
/// * Current directory does not exist.
/// * There are insufficient permissions to access the current directory.
/// * The encoding used to parse the current directory failed to parse.
/// * The current directory was not valid UTF8.
///
/// # Examples
///
/// ```
/// fn main() -> std::io::Result<()> {
///     let path = typed_path::utils::utf8_current_dir()?;
///     println!("The current directory is {}", path);
///     Ok(())
/// }
/// ```
pub fn utf8_current_dir() -> io::Result<Utf8PlatformPathBuf> {
    match Utf8PlatformPathBuf::from_bytes_path_buf(current_dir()?) {
        Ok(path) => Ok(path),
        Err(x) => Err(io::Error::new(io::ErrorKind::InvalidData, x)),
    }
}

/// Returns the full filesystem path of the current running executable as [`PlatformPathBuf`].
///
/// # Errors
///
/// Returns an [`Err`] if unable to parse the path with the platform encoding.
///
/// Additionally, returns a [`Err`] if, as [`env::current_exe`] states,
/// a related filesystem operation or syscall fails.
///
///
/// # Examples
///
/// ```
/// fn main() -> std::io::Result<()> {
///     let path = typed_path::utils::current_exe()?;
///     println!("The current executable path is {}", path.display());
///     Ok(())
/// }
/// ```
pub fn current_exe() -> io::Result<PlatformPathBuf> {
    let std_current_exe = env::current_exe()?;

    match PlatformPathBuf::try_from(std_current_exe) {
        Ok(path) => Ok(path),
        Err(_) => Err(io::Error::new(io::ErrorKind::InvalidData, "wrong encoding")),
    }
}

/// Returns the full filesystem path of the current running executable as [`Utf8PlatformPathBuf`].
///
/// # Errors
///
/// Returns an [`Err`] if unable to parse the path with the platform encoding
/// or the path was not valid UTF8.
///
/// Additionally, returns a [`Err`] if, as [`env::current_exe`] states,
/// a related filesystem operation or syscall fails.
///
///
/// # Examples
///
/// ```
/// fn main() -> std::io::Result<()> {
///     let path = typed_path::utils::utf8_current_exe()?;
///     println!("The current executable path is {}", path);
///     Ok(())
/// }
/// ```
pub fn utf8_current_exe() -> io::Result<Utf8PlatformPathBuf> {
    let typed_current_exe = current_exe()?;

    match Utf8PlatformPathBuf::from_bytes_path_buf(typed_current_exe) {
        Ok(path) => Ok(path),
        Err(error) => Err(io::Error::new(io::ErrorKind::InvalidData, error)),
    }
}

/// Returns the path of a temporary directory as [`PlatformPathBuf`].
///
/// # Errors
///
/// Returns an [`Err`] if unable to parse the path with the platform encoding.
///
///
/// # Examples
///
/// ```
/// fn main() -> std::io::Result<()> {
///     let path = typed_path::utils::temp_dir()?;
///     println!("The temporary directory path is {}", path.display());
///     Ok(())
/// }
/// ```
pub fn temp_dir() -> io::Result<PlatformPathBuf> {
    let std_temp_dir = env::temp_dir();

    match PlatformPathBuf::try_from(std_temp_dir) {
        Ok(path) => Ok(path),
        Err(_) => Err(io::Error::new(io::ErrorKind::InvalidData, "wrong encoding")),
    }
}

/// Returns the path of a temporary directory as [`Utf8PlatformPathBuf`].
///
/// # Errors
///
/// Returns an [`Err`] if unable to parse the path with the platform encoding
/// or the path was not valid UTF8.
///
///
/// # Examples
///
/// ```
/// fn main() -> std::io::Result<()> {
///     let path = typed_path::utils::utf8_temp_dir()?;
///     println!("The temporary directory path is {}", path);
///     Ok(())
/// }
/// ```
pub fn utf8_temp_dir() -> io::Result<Utf8PlatformPathBuf> {
    let typed_temp_dir = temp_dir()?;

    match Utf8PlatformPathBuf::from_bytes_path_buf(typed_temp_dir) {
        Ok(path) => Ok(path),
        Err(error) => Err(io::Error::new(io::ErrorKind::InvalidData, error)),
    }
}
