//! Unix `pipe` wrapper for `LibAFL`
#[cfg(feature = "std")]
use std::{
    io::{self, ErrorKind, PipeReader, PipeWriter, Read, Write},
    os::unix::io::RawFd,
};

#[cfg(feature = "std")]
use crate::Error;

/// A unix pipe wrapper for `LibAFL`
#[cfg(feature = "std")]
#[derive(Debug)]
pub struct Pipe {
    /// The read end of the pipe
    read_end: Option<PipeReader>,
    /// The write end of the pipe
    write_end: Option<PipeWriter>,
}

#[cfg(feature = "std")]
impl Clone for Pipe {
    fn clone(&self) -> Self {
        // try_clone only fails if we run out of fds (dup2) so this should be rather safe
        let read_end = self
            .read_end
            .as_ref()
            .map(PipeReader::try_clone)
            .transpose()
            .expect("fail to clone read_end");
        let write_end = self
            .write_end
            .as_ref()
            .map(PipeWriter::try_clone)
            .transpose()
            .expect("fail to clone read_end");

        Self {
            read_end,
            write_end,
        }
    }
}

#[cfg(feature = "std")]
impl Pipe {
    /// Create a new `Unix` pipe
    pub fn new() -> Result<Self, Error> {
        let (read_end, write_end) = io::pipe()?;
        Ok(Self {
            read_end: Some(read_end),
            write_end: Some(write_end),
        })
    }

    /// Close the read end of a pipe
    pub fn close_read_end(&mut self) {
        // `OwnedFd` closes on Drop
        self.read_end = None;
    }

    /// Close the write end of a pipe
    pub fn close_write_end(&mut self) {
        // `OwnedFd` closes on Drop
        self.write_end = None;
    }

    /// The read end
    #[must_use]
    pub fn read_end(&self) -> Option<RawFd> {
        self.read_end.as_ref().map(std::os::fd::AsRawFd::as_raw_fd)
    }

    /// The write end
    #[must_use]
    pub fn write_end(&self) -> Option<RawFd> {
        self.write_end.as_ref().map(std::os::fd::AsRawFd::as_raw_fd)
    }
}

#[cfg(feature = "std")]
impl Read for Pipe {
    /// Reads a few bytes
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, io::Error> {
        match self.read_end.as_mut() {
            Some(read_end) => read_end.read(buf),
            None => Err(io::Error::new(
                ErrorKind::BrokenPipe,
                "Read pipe end was already closed",
            )),
        }
    }
}

#[cfg(feature = "std")]
impl Write for Pipe {
    /// Writes a few bytes
    fn write(&mut self, buf: &[u8]) -> Result<usize, io::Error> {
        match self.write_end.as_mut() {
            Some(write_end) => Ok(write_end.write(buf)?),
            None => Err(io::Error::new(
                ErrorKind::BrokenPipe,
                "Write pipe end was already closed",
            )),
        }
    }

    fn flush(&mut self) -> Result<(), io::Error> {
        Ok(())
    }
}
