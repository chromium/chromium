//! List directory contents

use crate::errno::Errno;
use crate::fcntl::{self, OFlag};
use crate::sys;
use crate::{NixPath, Result};
use cfg_if::cfg_if;
use std::ffi::{CStr, CString};
use std::os::unix::io::{AsRawFd, IntoRawFd, RawFd};
use std::ptr;

/// An open directory.
///
/// This is a lower-level interface than [`std::fs::ReadDir`]. Notable differences:
///    * can be opened from a file descriptor (as returned by [`openat`][openat],
///      perhaps before knowing if the path represents a file or directory).
///    * implements [`AsFd`][AsFd], so it can be passed to [`fstat`][fstat],
///      [`openat`][openat], etc. The file descriptor continues to be owned by the
///      `Dir`, so callers must not keep a `RawFd` after the `Dir` is dropped.
///    * can be iterated through multiple times without closing and reopening the file
///      descriptor. Each iteration rewinds when finished.
///    * returns entries for `.` (current directory) and `..` (parent directory).
///    * returns entries' names as a [`CStr`] (no allocation or conversion beyond whatever libc
///      does).
///
/// [AsFd]: std::os::fd::AsFd
/// [fstat]: crate::sys::stat::fstat
/// [openat]: crate::fcntl::openat
/// [cstr]: std::ffi::CStr
///
/// # Examples
///
/// Traverse the current directory, and print entries' names:
///
/// ```
/// use nix::dir::Dir;
/// use nix::fcntl::OFlag;
/// use nix::sys::stat::Mode;
///
/// let mut cwd = Dir::open(".", OFlag::O_RDONLY | OFlag::O_CLOEXEC, Mode::empty()).unwrap();
/// for res_entry in cwd.iter() {
///     let entry = res_entry.unwrap();
///     println!("File name: {}", entry.file_name().to_string_lossy());
/// }
/// ```
#[derive(Debug, Eq, Hash, PartialEq)]
pub struct Dir(ptr::NonNull<libc::DIR>);

impl Dir {
    /// Opens the given path as with `fcntl::open`.
    pub fn open<P: ?Sized + NixPath>(
        path: &P,
        oflag: OFlag,
        mode: sys::stat::Mode,
    ) -> Result<Self> {
        let fd = fcntl::open(path, oflag, mode)?;
        Dir::from_fd(fd)
    }

    /// Opens the given path as with `fcntl::openat`.
    pub fn openat<Fd: std::os::fd::AsFd, P: ?Sized + NixPath>(
        dirfd: Fd,
        path: &P,
        oflag: OFlag,
        mode: sys::stat::Mode,
    ) -> Result<Self> {
        let fd = fcntl::openat(dirfd, path, oflag, mode)?;
        Dir::from_fd(fd)
    }

    /// Converts from a descriptor-based object, closing the descriptor on success or failure.
    ///
    /// # Safety
    ///
    /// It is only safe if `fd` is an owned file descriptor.
    #[inline]
    #[deprecated(
        since = "0.30.0",
        note = "Deprecate this since it is not I/O-safe, use from_fd instead."
    )]
    pub unsafe fn from<F: IntoRawFd>(fd: F) -> Result<Self> {
        use std::os::fd::FromRawFd;
        use std::os::fd::OwnedFd;

        // SAFETY:
        //
        // This is indeed unsafe is `fd` it not an owned fd.
        let owned_fd = unsafe { OwnedFd::from_raw_fd(fd.into_raw_fd()) };
        Dir::from_fd(owned_fd)
    }

    /// Converts from a file descriptor, closing it on failure.
    ///
    /// # Examples
    ///
    /// `ENOTDIR` would be returned if `fd` does not refer to a directory:
    ///
    /// ```should_panic
    /// use std::os::fd::OwnedFd;
    /// use nix::dir::Dir;
    ///
    /// let temp_file = tempfile::tempfile().unwrap();
    /// let temp_file_fd: OwnedFd = temp_file.into();
    /// let never = Dir::from_fd(temp_file_fd).unwrap();
    /// ```
    #[doc(alias("fdopendir"))]
    pub fn from_fd(fd: std::os::fd::OwnedFd) -> Result<Self> {
        // take the ownership as the constructed `Dir` is now the owner
        let raw_fd = fd.into_raw_fd();
        let d = ptr::NonNull::new(unsafe { libc::fdopendir(raw_fd) })
            .ok_or(Errno::last())?;
        Ok(Dir(d))
    }

    /// Returns an iterator of `Result<Entry>` which rewinds when finished.
    pub fn iter(&mut self) -> Iter<'_> {
        Iter(self)
    }
}

// `Dir` is not `Sync` because it's unsafe to call `readdir` simultaneously from multiple threads.
//
// `Dir` is safe to pass from one thread to another, as it's not reference-counted.
unsafe impl Send for Dir {}

impl std::os::fd::AsFd for Dir {
    fn as_fd(&self) -> std::os::fd::BorrowedFd<'_> {
        let raw_fd = self.as_raw_fd();

        // SAFETY:
        //
        // `raw_fd` should be open and valid for the lifetime of the returned
        // `BorrowedFd` as it is extracted from `&self`.
        unsafe { std::os::fd::BorrowedFd::borrow_raw(raw_fd) }
    }
}

impl AsRawFd for Dir {
    fn as_raw_fd(&self) -> RawFd {
        unsafe { libc::dirfd(self.0.as_ptr()) }
    }
}

impl Drop for Dir {
    fn drop(&mut self) {
        let e = Errno::result(unsafe { libc::closedir(self.0.as_ptr()) });
        if !std::thread::panicking() && e == Err(Errno::EBADF) {
            panic!("Closing an invalid file descriptor!");
        };
    }
}

// The pass by mut is technically needless only because the inner NonNull is
// Copy.  But we are actually mutating the Dir, so we pass by mut.
#[allow(clippy::needless_pass_by_ref_mut)]
fn readdir(dir: &mut Dir) -> Option<Result<Entry>> {
    Errno::clear();
    unsafe {
        let de = libc::readdir(dir.0.as_ptr());
        if de.is_null() {
            if Errno::last_raw() == 0 {
                // EOF
                None
            } else {
                Some(Err(Errno::last()))
            }
        } else {
            Some(Ok(Entry::from_raw(&*de)))
        }
    }
}

/// Return type of [`Dir::iter`].
#[derive(Debug, Eq, Hash, PartialEq)]
pub struct Iter<'d>(&'d mut Dir);

impl Iterator for Iter<'_> {
    type Item = Result<Entry>;

    fn next(&mut self) -> Option<Self::Item> {
        readdir(self.0)
    }
}

impl Drop for Iter<'_> {
    fn drop(&mut self) {
        unsafe { libc::rewinddir((self.0).0.as_ptr()) }
    }
}

/// The return type of [Dir::into_iter]
#[derive(Debug, Eq, Hash, PartialEq)]
pub struct OwningIter(Dir);

impl Iterator for OwningIter {
    type Item = Result<Entry>;

    fn next(&mut self) -> Option<Self::Item> {
        readdir(&mut self.0)
    }
}

/// The file descriptor continues to be owned by the `OwningIter`,
/// so callers must not keep a `RawFd` after the `OwningIter` is dropped.
impl AsRawFd for OwningIter {
    fn as_raw_fd(&self) -> RawFd {
        self.0.as_raw_fd()
    }
}

impl IntoIterator for Dir {
    type Item = Result<Entry>;
    type IntoIter = OwningIter;

    /// Creates a owning iterator, that is, one that takes ownership of the
    /// `Dir`. The `Dir` cannot be used after calling this.  This can be useful
    /// when you have a function that both creates a `Dir` instance and returns
    /// an `Iterator`.
    ///
    /// Example:
    ///
    /// ```
    /// use nix::{dir::Dir, fcntl::OFlag, sys::stat::Mode};
    /// use std::{iter::Iterator, string::String};
    ///
    /// fn ls_upper(dirname: &str) -> impl Iterator<Item=String> {
    ///     let d = Dir::open(dirname, OFlag::O_DIRECTORY, Mode::S_IXUSR).unwrap();
    ///     d.into_iter().map(|x| x.unwrap().file_name().as_ref().to_string_lossy().to_ascii_uppercase())
    /// }
    /// ```
    fn into_iter(self) -> Self::IntoIter {
        OwningIter(self)
    }
}

/// A directory entry, similar to `std::fs::DirEntry`.
///
/// Note that unlike the std version, this may represent the `.` or `..` entries.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct Entry {
    ino: u64,
    type_: Option<Type>,
    // On some platforms libc::dirent is a "flexible-length structure", where there may be
    // data beyond the end of the struct definition.  So it isn't possible to copy it and subsequently
    // dereference the copy's d_name field.  Nor is it possible for Entry to wrap a &libc::dirent.
    // At least, not if it is to work with the Iterator trait.  Because the Entry would need to
    // maintain a mutable reference to the Dir, which would conflict with the iterator's mutable
    // reference to the same Dir.  So we're forced to copy the name here.
    name: CString,
}

/// Type of file referenced by a directory entry
#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
pub enum Type {
    /// FIFO (Named pipe)
    Fifo,
    /// Character device
    CharacterDevice,
    /// Directory
    Directory,
    /// Block device
    BlockDevice,
    /// Regular file
    File,
    /// Symbolic link
    Symlink,
    /// Unix-domain socket
    Socket,
}

impl Entry {
    /// Returns the inode number (`d_ino`) of the underlying `dirent`.
    pub fn ino(&self) -> u64 {
        self.ino
    }

    /// Returns the bare file name of this directory entry without any other leading path component.
    pub fn file_name(&self) -> &CStr {
        self.name.as_c_str()
    }

    /// Returns the type of this directory entry, if known.
    ///
    /// See platform `readdir(3)` or `dirent(5)` manpage for when the file type is known;
    /// notably, some Linux filesystems don't implement this. The caller should use `stat` or
    /// `fstat` if this returns `None`.
    pub fn file_type(&self) -> Option<Type> {
        self.type_
    }

    #[allow(clippy::useless_conversion)] // Not useless on all OSes
    // The cast is not unnecessary on all platforms.
    #[allow(clippy::unnecessary_cast)]
    fn from_raw(de: &libc::dirent) -> Self {
        cfg_if! {
            if #[cfg(any(target_os = "aix",
                         target_os = "emscripten",
                         target_os = "fuchsia",
                         target_os = "haiku",
                         target_os = "hurd",
                         target_os = "cygwin",
                         solarish,
                         linux_android,
                         apple_targets))] {
                let ino = de.d_ino as u64;
            } else {
                let ino = u64::from(de.d_fileno);
            }
        }
        cfg_if! {
            if #[cfg(not(any(solarish, target_os = "aix", target_os = "haiku")))] {
                let type_ = match de.d_type {
                    libc::DT_FIFO => Some(Type::Fifo),
                    libc::DT_CHR => Some(Type::CharacterDevice),
                    libc::DT_DIR => Some(Type::Directory),
                    libc::DT_BLK => Some(Type::BlockDevice),
                    libc::DT_REG => Some(Type::File),
                    libc::DT_LNK => Some(Type::Symlink),
                    libc::DT_SOCK => Some(Type::Socket),
                    /* libc::DT_UNKNOWN | */ _ => None,
                };
            } else {
                // illumos, Solaris, and Haiku systems do not have the d_type member at all:
                #[cfg(any(solarish, target_os = "aix", target_os = "haiku"))]
                let type_ = None;
            }
        }
        // Annoyingly, since libc::dirent is open-ended, the easiest way to read the name field in
        // Rust is to treat it like a pointer.
        // Safety: safe because we knod that de.d_name is in valid memory.
        let name = unsafe { CStr::from_ptr(de.d_name.as_ptr()) }.to_owned();

        Entry { ino, type_, name }
    }
}
