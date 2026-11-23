use crate::io;
use crate::io::{BufRead, Write};
use alloc::vec::Vec;
use core::mem;

use crate::{
    Compress, CompressError, Decompress, DecompressError, FlushCompress, FlushDecompress, Status,
};

#[derive(Debug)]
pub struct Writer<W: Write, D: Ops> {
    obj: Option<W>,
    pub data: D,
    buf: Vec<u8>,
    finished: bool,
    buf_pos: usize,
}

pub trait Ops {
    type Error: Into<io::Error>;
    type Flush: Flush;
    /// Whether running out of input before `Status::StreamEnd` means the
    /// stream is incomplete, as is the case for decompression.
    const REQUIRES_STREAM_END: bool;
    fn total_in(&self) -> u64;
    fn total_out(&self) -> u64;
    fn run(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: Self::Flush,
    ) -> Result<Status, Self::Error>;
    fn run_vec(
        &mut self,
        input: &[u8],
        output: &mut Vec<u8>,
        flush: Self::Flush,
    ) -> Result<Status, Self::Error>;
    fn flush<W: Write>(writer: &mut Writer<W, Self>) -> io::Result<()>
    where
        Self: Sized;
}

impl Ops for Compress {
    type Error = CompressError;
    type Flush = FlushCompress;
    /// Unlike decompression, compression accepts arbitrary, unframed input and
    /// creates the stream's ending with `FlushCompress::Finish`. Exhausting the
    /// input cannot indicate truncation, so `Writer::finish` keeps the encoder's
    /// existing no-more-output completion rule instead of requiring `StreamEnd`.
    const REQUIRES_STREAM_END: bool = false;
    fn total_in(&self) -> u64 {
        self.total_in()
    }
    fn total_out(&self) -> u64 {
        self.total_out()
    }
    fn run(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushCompress,
    ) -> Result<Status, CompressError> {
        self.compress(input, output, flush)
    }
    fn run_vec(
        &mut self,
        input: &[u8],
        output: &mut Vec<u8>,
        flush: FlushCompress,
    ) -> Result<Status, CompressError> {
        self.compress_vec(input, output, flush)
    }

    fn flush<W: Write>(writer: &mut Writer<W, Self>) -> io::Result<()> {
        loop {
            writer.dump()?;
            writer
                .data
                .compress_vec(&[], &mut writer.buf, FlushCompress::Sync)?;
            if writer.buf.len() < writer.buf.capacity() {
                return writer.dump();
            }
            // A full buffer may leave the sync flush incomplete; repeat it with fresh space.
        }
    }
}

impl Ops for Decompress {
    type Error = DecompressError;
    type Flush = FlushDecompress;
    /// Unlike compression, decompression must find the stream's ending in the
    /// supplied input; `FlushDecompress::Finish` cannot create missing bytes.
    /// Once `Writer::finish` can produce no more output, only `StreamEnd` proves
    /// the input was complete. Otherwise, it must report `UnexpectedEof`.
    const REQUIRES_STREAM_END: bool = true;
    fn total_in(&self) -> u64 {
        self.total_in()
    }
    fn total_out(&self) -> u64 {
        self.total_out()
    }
    fn run(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushDecompress,
    ) -> Result<Status, DecompressError> {
        self.decompress(input, output, flush)
    }
    fn run_vec(
        &mut self,
        input: &[u8],
        output: &mut Vec<u8>,
        flush: FlushDecompress,
    ) -> Result<Status, DecompressError> {
        self.decompress_vec(input, output, flush)
    }

    fn flush<W: Write>(writer: &mut Writer<W, Self>) -> io::Result<()> {
        writer.dump()?;
        writer
            .data
            .decompress_vec(&[], &mut writer.buf, FlushDecompress::Sync)?;

        // Decoders can leave pending output even after producing a short chunk.
        loop {
            writer.dump()?;
            let before = writer.data.total_out();
            writer
                .data
                .decompress_vec(&[], &mut writer.buf, FlushDecompress::None)?;
            if before == writer.data.total_out() {
                return Ok(());
            }
        }
    }
}

pub trait Flush {
    fn none() -> Self;
    fn finish() -> Self;
}

impl Flush for FlushCompress {
    fn none() -> Self {
        FlushCompress::None
    }

    fn finish() -> Self {
        FlushCompress::Finish
    }
}

impl Flush for FlushDecompress {
    fn none() -> Self {
        FlushDecompress::None
    }

    fn finish() -> Self {
        FlushDecompress::Finish
    }
}

pub fn read<R, D>(obj: &mut R, data: &mut D, dst: &mut [u8]) -> io::Result<usize>
where
    R: BufRead,
    D: Ops,
{
    loop {
        let (read, consumed, ret, eof);
        {
            let input = obj.fill_buf()?;
            eof = input.is_empty();
            let before_out = data.total_out();
            let before_in = data.total_in();
            let flush = if eof {
                D::Flush::finish()
            } else {
                D::Flush::none()
            };
            ret = data.run(input, dst, flush);
            read = (data.total_out() - before_out) as usize;
            consumed = (data.total_in() - before_in) as usize;
        }
        obj.consume(consumed);

        match ret {
            // If we haven't read any data and we haven't hit EOF yet,
            // then we need to keep asking for more data because if we
            // return that 0 bytes of data have been read then it will
            // be interpreted as EOF.
            Ok(Status::Ok | Status::BufError) if read == 0 && !eof && !dst.is_empty() => continue,
            // If we haven't read any data and we have hit EOF, then the
            // deflate stream is incomplete.
            Ok(Status::Ok | Status::BufError) if read == 0 && eof && !dst.is_empty() => {
                return Err(io::Error::new(
                    io::ErrorKind::UnexpectedEof,
                    "incomplete deflate stream",
                ));
            }
            Ok(Status::Ok | Status::BufError | Status::StreamEnd) => return Ok(read),

            Err(..) => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "corrupt deflate stream",
                ))
            }
        }
    }
}

impl<W: Write, D: Ops> Writer<W, D> {
    pub fn new(w: W, d: D) -> Writer<W, D> {
        Writer {
            obj: Some(w),
            data: d,
            buf: Vec::with_capacity(32 * 1024),
            finished: false,
            buf_pos: 0,
        }
    }

    pub fn finish(&mut self) -> io::Result<()> {
        self.finish_inner(false)
    }

    pub(crate) fn drain(&mut self) -> io::Result<()> {
        self.finish_inner(true)
    }

    fn finish_inner(&mut self, allow_incomplete: bool) -> io::Result<()> {
        if self.finished {
            return self.dump();
        }

        loop {
            self.dump()?;

            let before = self.data.total_out();
            let status = self
                .data
                .run_vec(&[], &mut self.buf, Flush::finish())
                .map_err(Into::into)?;
            if status == Status::StreamEnd {
                self.finished = true;
                return self.dump();
            }
            if before == self.data.total_out() {
                // No more output and no end of stream: the input was
                // truncated, mirroring the check in `read` above.
                if D::REQUIRES_STREAM_END && !allow_incomplete {
                    return Err(io::Error::new(
                        io::ErrorKind::UnexpectedEof,
                        "incomplete deflate stream",
                    ));
                }
                return Ok(());
            }
        }
    }

    pub fn replace(&mut self, w: W) -> W {
        self.buf.clear();
        self.finished = false;
        self.buf_pos = 0;
        mem::replace(self.get_mut(), w)
    }

    pub fn get_ref(&self) -> &W {
        self.obj.as_ref().unwrap()
    }

    pub fn get_mut(&mut self) -> &mut W {
        self.obj.as_mut().unwrap()
    }

    // Note that this should only be called if the outer object is just about
    // to be consumed!
    //
    // (e.g. an implementation of `into_inner`)
    pub fn take_inner(&mut self) -> W {
        self.obj.take().unwrap()
    }

    pub fn is_present(&self) -> bool {
        self.obj.is_some()
    }

    // Returns total written bytes and status of underlying codec
    pub(crate) fn write_with_status(&mut self, buf: &[u8]) -> io::Result<(usize, Status)> {
        // miniz isn't guaranteed to actually write any of the buffer provided,
        // it may be in a flushing mode where it's just giving us data before
        // we're actually giving it any data. We don't want to spuriously return
        // `Ok(0)` when possible as it will cause calls to write_all() to fail.
        // As a result we execute this in a loop to ensure that we try our
        // darndest to write the data.
        loop {
            self.dump()?;

            let before_in = self.data.total_in();
            let ret = self.data.run_vec(buf, &mut self.buf, D::Flush::none());
            let written = (self.data.total_in() - before_in) as usize;
            let is_stream_end = matches!(ret, Ok(Status::StreamEnd));

            // Without input consumed or output to drain, retrying can loop forever.
            if !buf.is_empty()
                && written == 0
                && ret.is_ok()
                && !is_stream_end
                && !self.buf.is_empty()
            {
                continue;
            }
            if is_stream_end {
                self.finished = true;
            }
            return match ret {
                Ok(st) => match st {
                    Status::Ok | Status::BufError | Status::StreamEnd => Ok((written, st)),
                },
                Err(..) => Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "corrupt deflate stream",
                )),
            };
        }
    }

    fn dump(&mut self) -> io::Result<()> {
        let obj = self
            .obj
            .as_mut()
            .expect("inner writer should only be taken when the outer writer is consumed");
        while self.buf_pos < self.buf.len() {
            let n = obj.write(&self.buf[self.buf_pos..])?;
            if n == 0 {
                return Err(io::ErrorKind::WriteZero.into());
            }
            self.buf_pos += n;
        }
        self.buf.clear();
        self.buf_pos = 0;
        Ok(())
    }
}

impl<W: Write, D: Ops> Write for Writer<W, D> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.write_with_status(buf).map(|res| res.0)
    }

    fn flush(&mut self) -> io::Result<()> {
        D::flush(self)?;

        self.obj
            .as_mut()
            .expect("inner writer should only be taken when the outer writer is consumed")
            .flush()
    }
}

impl<W: Write, D: Ops> Drop for Writer<W, D> {
    fn drop(&mut self) {
        if self.obj.is_some() {
            let _ = self.finish();
        }
    }
}
