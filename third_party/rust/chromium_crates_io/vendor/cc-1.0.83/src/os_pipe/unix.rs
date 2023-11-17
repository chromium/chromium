use std::{
    fs::File,
    io,
    os::{raw::c_int, unix::io::FromRawFd},
};

pub(super) fn pipe() -> io::Result<(File, File)> {
    let mut fds = [0; 2];

    // The only known way right now to create atomically set the CLOEXEC flag is
    // to use the `pipe2` syscall. This was added to Linux in 2.6.27, glibc 2.9
    // and musl 0.9.3, and some other targets also have it.
    #[cfg(any(
        target_os = "dragonfly",
        target_os = "freebsd",
        target_os = "linux",
        target_os = "netbsd",
        target_os = "openbsd",
        target_os = "redox"
    ))]
    {
        unsafe {
            cvt(libc::pipe2(fds.as_mut_ptr(), libc::O_CLOEXEC))?;
        }
    }

    #[cfg(not(any(
        target_os = "dragonfly",
        target_os = "freebsd",
        target_os = "linux",
        target_os = "netbsd",
        target_os = "openbsd",
        target_os = "redox"
    )))]
    {
        unsafe {
            cvt(libc::pipe(fds.as_mut_ptr()))?;
        }

        cloexec::set_cloexec(fds[0])?;
        cloexec::set_cloexec(fds[1])?;
    }

    unsafe { Ok((File::from_raw_fd(fds[0]), File::from_raw_fd(fds[1]))) }
}

fn cvt(t: c_int) -> io::Result<c_int> {
    if t == -1 {
        Err(io::Error::last_os_error())
    } else {
        Ok(t)
    }
}

#[cfg(not(any(
    target_os = "dragonfly",
    target_os = "freebsd",
    target_os = "linux",
    target_os = "netbsd",
    target_os = "openbsd",
    target_os = "redox"
)))]
mod cloexec {
    use super::{c_int, cvt, io};

    #[cfg(not(any(
        target_env = "newlib",
        target_os = "solaris",
        target_os = "illumos",
        target_os = "emscripten",
        target_os = "fuchsia",
        target_os = "l4re",
        target_os = "linux",
        target_os = "haiku",
        target_os = "redox",
        target_os = "vxworks",
        target_os = "nto",
    )))]
    pub(super) fn set_cloexec(fd: c_int) -> io::Result<()> {
        unsafe {
            cvt(libc::ioctl(fd, libc::FIOCLEX))?;
        }

        Ok(())
    }

    #[cfg(any(
        all(
            target_env = "newlib",
            not(any(target_os = "espidf", target_os = "horizon"))
        ),
        target_os = "solaris",
        target_os = "illumos",
        target_os = "emscripten",
        target_os = "fuchsia",
        target_os = "l4re",
        target_os = "linux",
        target_os = "haiku",
        target_os = "redox",
        target_os = "vxworks",
        target_os = "nto",
    ))]
    pub(super) fn set_cloexec(fd: c_int) -> io::Result<()> {
        unsafe {
            let previous = cvt(libc::fcntl(fd, libc::F_GETFD))?;
            let new = previous | libc::FD_CLOEXEC;
            if new != previous {
                cvt(libc::fcntl(fd, libc::F_SETFD, new))?;
            }
        }

        Ok(())
    }

    // FD_CLOEXEC is not supported in ESP-IDF and Horizon OS but there's no need to,
    // because neither supports spawning processes.
    #[cfg(any(target_os = "espidf", target_os = "horizon"))]
    pub(super) fn set_cloexec(_fd: c_int) -> io::Result<()> {
        Ok(())
    }
}
