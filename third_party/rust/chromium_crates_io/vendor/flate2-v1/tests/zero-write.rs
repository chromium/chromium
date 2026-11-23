use std::io::{self, Write};

#[test]
fn zero_write_is_error() {
    let mut buf = [0u8];
    let writer = flate2::write::DeflateEncoder::new(&mut buf[..], flate2::Compression::default());
    assert!(writer.finish().is_err());
}

#[derive(Debug)]
struct AlwaysZeroThenError {
    writes: usize,
    max_zero_writes: usize,
}

impl AlwaysZeroThenError {
    fn new(max_zero_writes: usize) -> Self {
        AlwaysZeroThenError {
            writes: 0,
            max_zero_writes,
        }
    }
}

impl Write for AlwaysZeroThenError {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if buf.is_empty() {
            return Ok(0);
        }

        self.writes += 1;
        if self.writes > self.max_zero_writes {
            Err(io::Error::new(
                io::ErrorKind::Other,
                "gzip encoder retried a zero-length write",
            ))
        } else {
            Ok(0)
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

#[derive(Debug)]
struct ZeroOnGzipFooterThenError {
    footer_writes: usize,
    max_zero_footer_writes: usize,
}

impl ZeroOnGzipFooterThenError {
    fn new(max_zero_footer_writes: usize) -> Self {
        ZeroOnGzipFooterThenError {
            footer_writes: 0,
            max_zero_footer_writes,
        }
    }
}

impl Write for ZeroOnGzipFooterThenError {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if buf.is_empty() {
            return Ok(0);
        }

        if buf == [0; 8] {
            self.footer_writes += 1;
            if self.footer_writes > self.max_zero_footer_writes {
                Err(io::Error::new(
                    io::ErrorKind::Other,
                    "gzip encoder retried a zero-length footer write",
                ))
            } else {
                Ok(0)
            }
        } else {
            Ok(buf.len())
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

#[test]
fn gzip_header_zero_write_is_error() {
    // GzEncoder used to spin in write_header when the wrapped writer returned
    // Ok(0) for the non-empty gzip header buffer: the header was not drained,
    // so the loop retried the same write forever. This bounded writer turns
    // that spin into an ordinary error so the test can catch the missing
    // WriteZero handling without hanging.
    //
    // Three zero-length writes is arbitrary but enough to show the old code
    // was retrying instead of treating Ok(0) on a non-empty buffer as
    // WriteZero.
    let writer = AlwaysZeroThenError::new(3);
    let mut encoder = flate2::write::GzEncoder::new(writer, flate2::Compression::default());

    let err = encoder.try_finish().unwrap_err();
    assert_eq!(err.kind(), io::ErrorKind::WriteZero);
}

#[test]
fn gzip_footer_zero_write_is_error() {
    // GzEncoder also used to spin while writing the 8-byte gzip footer. A
    // writer that accepts the header and deflate payload but returns Ok(0) for
    // the empty-stream footer (CRC32 0, ISIZE 0) left crc_bytes_written
    // unchanged, so try_finish retried the same footer slice forever.
    //
    // Three zero-length footer writes is arbitrary but enough to show the old
    // code was retrying the same footer slice instead of returning WriteZero.
    let writer = ZeroOnGzipFooterThenError::new(3);
    let mut encoder = flate2::write::GzEncoder::new(writer, flate2::Compression::default());

    let err = encoder.try_finish().unwrap_err();
    assert_eq!(err.kind(), io::ErrorKind::WriteZero);
}
