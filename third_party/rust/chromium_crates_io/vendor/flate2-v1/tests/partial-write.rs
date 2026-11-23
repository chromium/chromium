use flate2::read::DeflateDecoder;
use flate2::write::DeflateEncoder;
use flate2::Compression;
use rand::rngs::StdRng;
use rand::{Rng, SeedableRng};
use std::io::{self, Read, Write};

/// Accepts at most seven bytes per write and returns one `Interrupted` error
/// after writing at least 1 KiB.
#[derive(Default)]
struct PartialWriter {
    output: Vec<u8>,
    interrupted: bool,
}

impl Write for PartialWriter {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if !self.interrupted && self.output.len() >= 1024 {
            self.interrupted = true;
            return Err(io::ErrorKind::Interrupted.into());
        }

        let len = buf.len().min(7);
        self.output.extend_from_slice(&buf[..len]);
        Ok(len)
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

#[test]
fn encoder_handles_partial_writes() {
    let input: Vec<u8> = StdRng::seed_from_u64(0x1234_5678_9abc_def0)
        .random_iter()
        .take(16 * 1024)
        .collect();

    let mut encoder = DeflateEncoder::new(PartialWriter::default(), Compression::fast());
    encoder.write_all(&input).unwrap();
    let mut flush_was_interrupted = false;
    loop {
        match encoder.flush() {
            Err(error) if error.kind() == io::ErrorKind::Interrupted => {
                flush_was_interrupted = true;
                continue;
            }
            result => result.unwrap(),
        }
        break;
    }
    let writer = encoder.finish().unwrap();
    assert!(
        flush_was_interrupted,
        "flushing never surfaced the downstream interruption"
    );

    let mut decoded = Vec::new();
    DeflateDecoder::new(writer.output.as_slice())
        .read_to_end(&mut decoded)
        .unwrap();
    assert_eq!(decoded, input, "decoded output differs from the input");
}

/// Returns `WouldBlock` once after each 8 KiB of output, through 32 KiB.
struct BlockingWriter {
    output: Vec<u8>,
    max_write: usize,
    blocked: usize,
}

impl Write for BlockingWriter {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if buf.is_empty() {
            return Ok(0);
        }

        let mut len = buf.len().min(self.max_write);
        if self.blocked < 4 {
            let until_block = (self.blocked + 1) * 8 * 1024 - self.output.len();
            if until_block == 0 {
                self.blocked += 1;
                return Err(io::ErrorKind::WouldBlock.into());
            }
            len = len.min(until_block);
        }
        self.output.extend_from_slice(&buf[..len]);
        Ok(len)
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

#[test]
fn decoder_handles_partial_writes_and_would_block() {
    fn retry<T>(retries: &mut usize, mut operation: impl FnMut() -> io::Result<T>) -> T {
        loop {
            match operation() {
                Err(error) if error.kind() == io::ErrorKind::WouldBlock => *retries += 1,
                result => return result.unwrap(),
            }
        }
    }

    // Cross the decoder's 32 KiB output buffer boundary.
    let input: Vec<u8> = StdRng::seed_from_u64(0x1234_5678_9abc_def0)
        .random_iter()
        .take(33 * 1024)
        .collect();
    let mut encoder = DeflateEncoder::new(Vec::new(), Compression::fast());
    encoder.write_all(&input).unwrap();
    let compressed = encoder.finish().unwrap();

    for max_write in [7, 32 * 1024] {
        let mut decoder = flate2::write::DeflateDecoder::new(BlockingWriter {
            output: Vec::new(),
            max_write,
            blocked: 0,
        });
        let mut retries = 0;
        let mut remaining = compressed.as_slice();
        // Track input progress explicitly: write_all cannot resume after WouldBlock.
        while !remaining.is_empty() {
            let n = retry(&mut retries, || decoder.write(remaining));
            assert!(
                n > 0,
                "decoder stopped consuming input (max_write={})",
                max_write
            );
            remaining = &remaining[n..];
        }
        retry(&mut retries, || decoder.flush());
        retry(&mut retries, || decoder.try_finish());
        let writer = decoder.finish().unwrap();

        assert_eq!(
            retries, 4,
            "not all downstream errors reached the caller (max_write={max_write})"
        );
        assert_eq!(
            writer.output, input,
            "decoded output differs from the input (max_write={max_write})"
        );
    }
}
