#![cfg(test)]
#![cfg(feature="std")]
use std::io::{self, Read, Cursor};

extern crate brotli_decompressor;

static ENCODED: &'static [u8] = b"\x1b\x03)\x00\xa4\xcc\xde\xe2\xb3 vA\x00\x0c";

enum State { First, Second, Third, Fourth }
struct R(State);
impl Read for R {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match self.0 {
            State::First => {
                buf[0] = ENCODED[0];
                self.0 = State::Second;
                return Ok(1);
            }
            State::Second => {
                self.0 = State::Third;
                return Err(io::Error::new(io::ErrorKind::WouldBlock, "foo"));
            }
            State::Third => {
                self.0 = State::Fourth;
                buf[..ENCODED.len() - 1].copy_from_slice(&ENCODED[1..]);
                return Ok(ENCODED[1..].len());
            }
            State::Fourth => {
                return Ok(0);
            }
        }
    }
}
#[test]
fn test_would_block() {

    let mut d = brotli_decompressor::Decompressor::new(R(State::First), 8192);
    let mut b = [0; 8192];
    assert_eq!(d.read(&mut b).unwrap_err().kind(), io::ErrorKind::WouldBlock);
    assert!(d.read(&mut b).unwrap() != 0);
    println!("{}", String::from_utf8(b.to_vec()).unwrap());
    assert!(d.read(&mut b).unwrap() != 0);
    assert_eq!(d.read(&mut b).unwrap(), 0);
}

static ENCODED2: &'static [u8] = include_bytes!("ipsum.brotli");
static DECODED: &'static str = include_str!("ipsum.raw");

enum State2 {
    First,
    Second,
    Third,
    Fourth,
    Fifth,
    Sixth,
    Seventh,
    Eighth,
}

struct R2 {
    offset: usize,
    len: usize,
    state: State2,
}

impl R2 {
    fn new() -> R2 {
        R2 {
            offset: 0,
            len: 1,
            state: State2::First,
        }
    }
}
impl Read for R2 {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match self.state {
            State2::First => {
                self.state = State2::Second;
                let len = self.len;
                buf[..len].copy_from_slice(&ENCODED2[self.offset..self.offset+len]);
                self.offset += len;
                self.len = 100;
                return Ok(len);
            }
            State2::Second => {
                self.state = State2::Third;
                return Err(io::Error::new(io::ErrorKind::WouldBlock, "foo"));
            }
            State2::Third => {
                self.state = State2::Fourth;
                let len = self.len;
                buf[..len].copy_from_slice(&ENCODED2[self.offset..self.offset+len]);
                self.offset += len;
                self.len = 100;
                return Ok(len);
            }
            State2::Fourth => {
                self.state = State2::Fifth;
                return Err(io::Error::new(io::ErrorKind::WouldBlock, "foo"));
            }
            State2::Fifth => {
                self.state = State2::Sixth;
                let len = self.len;
                buf[..len].copy_from_slice(&ENCODED2[self.offset..self.offset+len]);
                self.offset += len;
                self.len = 100;
                return Ok(len);
            }
            State2::Sixth => {
                self.state = State2::Seventh;
                return Err(io::Error::new(io::ErrorKind::WouldBlock, "foo"));
            }
            State2::Seventh => {
                self.state = State2::Eighth;
                buf[..ENCODED2.len() - self.offset].copy_from_slice(&ENCODED2[self.offset..]);
                return Ok(ENCODED2.len() - self.offset);
            }
            State2::Eighth => {
                return Ok(0);
            }
        }
    }
}

#[test]
fn would_block_more() {
    // Reference synchronous decoding.
    let mut b = [0; 8192];
    let mut bytes = vec![];
    let mut d = brotli_decompressor::Decompressor::new(Cursor::new(ENCODED2), 8192);
    let read = d.read(&mut b).unwrap();
    assert!(read != 0);
    bytes.extend_from_slice(&b[0..read]);

    assert_eq!(d.read(&mut b).unwrap(), 0);
    let reference_decoded = String::from_utf8(bytes).unwrap();
    // Ensure synchronous decoding matches original input.
    assert_eq!(reference_decoded, DECODED);

    // Incremental decoding.
    let r = R2::new();
    let mut d = brotli_decompressor::Decompressor::new(r, 8192);
    let mut bytes = vec![];
    let mut b = [0; 8192];

    assert_eq!(d.read(&mut b).unwrap_err().kind(), io::ErrorKind::WouldBlock);

    assert_eq!(d.read(&mut b).unwrap_err().kind(), io::ErrorKind::WouldBlock);

    let read = d.read(&mut b).unwrap();
    assert!(read != 0);
    bytes.extend_from_slice(&b[0..read]);

    assert_eq!(d.read(&mut b).unwrap_err().kind(), io::ErrorKind::WouldBlock);

    let read = d.read(&mut b).unwrap();
    assert!(read != 0);
    bytes.extend_from_slice(&b[0..read]);

    assert_eq!(d.read(&mut b).unwrap(), 0);

    let decoded = String::from_utf8(bytes).unwrap();

    // Ensure incremental decoding matches original input after brotli decompressor is finished.
    assert_eq!(decoded, reference_decoded);
}

// Regression test for a missing `break` in the BROTLI_STATE_METABLOCK_DONE arm
// of BrotliDecompressStream that caused PADDING_2 errors (RFC 7932 §9.3:
// "the unused bits in the last byte must be zeros") to be silently dropped:
// the error code was set, control fell through into BROTLI_STATE_DONE, and
// WriteRingBuffer's return overwrote `result` with SUCCESS.
//
// This is the same valid 69-byte brotli stream encoding "the quick brown fox
// jumps over the lazy dog twice for redundancy and length" used by the C
// reference (`libbrotlidec`) and Go implementations (cbrotli / andybalholm)
// in their corresponding tests; both reject all corruptions below as
// PADDING_2 / _ERROR_FORMAT_PADDING_2.
#[test]
fn test_padding_2_rejection() {
    // Valid encoding of "the quick brown fox jumps over the lazy dog twice
    // for redundancy and length" produced by libbrotlidec at quality 5.
    let valid: &[u8] = &[
        0x1b, 0x4a, 0x00, 0x00, 0xc4, 0xf4, 0xa4, 0x69, 0xbd, 0x79, 0x25, 0x2d, 0x22, 0xb4, 0x52,
        0xea, 0x83, 0x0d, 0x38, 0x70, 0x68, 0xb2, 0x71, 0xc0, 0x41, 0x76, 0x1e, 0x36, 0xc6, 0xce,
        0x13, 0x84, 0xe8, 0x36, 0xf2, 0x2a, 0x0c, 0xe7, 0x89, 0x68, 0x7a, 0x04, 0x49, 0x2f, 0xaa,
        0xf7, 0x31, 0xa1, 0x9b, 0x0d, 0x48, 0xb7, 0xf0, 0x1f, 0x48, 0x33, 0x42, 0xa5, 0x9c, 0x31,
        0x26, 0x97, 0xa9, 0xc6, 0xbe, 0x67, 0x85, 0x52, 0x02,
    ];

    // Sanity: the unmodified stream decodes successfully.
    let mut d = brotli_decompressor::Decompressor::new(Cursor::new(valid), 4096);
    let mut decoded = Vec::new();
    d.read_to_end(&mut decoded).unwrap();
    assert_eq!(
        decoded,
        b"the quick brown fox jumps over the lazy dog twice for redundancy and length",
    );

    // Each of these single-bit flips lands in the final metablock's
    // byte-alignment padding region. A spec-conformant decoder must return
    // BROTLI_DECODER_ERROR_FORMAT_PADDING_2 for all of them.
    for &(offset, xor) in &[(13usize, 0x01u8), (23, 0x01), (33, 0x55)] {
        let mut corrupted = valid.to_vec();
        corrupted[offset] ^= xor;

        let mut d = brotli_decompressor::Decompressor::new(Cursor::new(corrupted.clone()), 4096);
        let mut sink = Vec::new();
        let err = d.read_to_end(&mut sink).map(|_| sink.clone()).err();
        let kind = err.as_ref().map(|e| e.kind());
        assert_eq!(
            kind,
            Some(io::ErrorKind::InvalidData),
            "decoder must reject padding-bit corruption at offset {} xor {:#x} \
             with InvalidData; got {:?}",
            offset, xor, err.as_ref().map_or_else(|| format!("Ok({:?})", sink), |e| format!("{:?}", e)),
        );
    }
}

// Found by the differential_dictionaries fuzzer: a corrupted stream whose
// final command overruns the declared metablock length
// (meta_block_remaining_len goes negative). The C implementation rejects it
// with BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2; this decoder used to
// accept it and emit 326 bytes.
#[test]
fn test_metablock_overrun_rejection() {
    let corrupted: &[u8] = &[
        0x03, 0xa0, 0x00, 0x00, 0x80, 0xaa, 0xaa, 0xaa, 0xea, 0xff, 0x70, 0x91, 0xc3, 0x4d, 0x2e,
        0x04, 0x77, 0x39, 0xdc, 0xe4, 0x70, 0x95, 0xc3, 0x4d, 0x0e, 0x57, 0x3d, 0xdc, 0xf4, 0x70,
        0xd5, 0xc3, 0x4d, 0x0f, 0x57, 0x3d, 0xdc, 0xf4, 0x70, 0xd5, 0xc3, 0x4d, 0x0f, 0x57, 0x3d,
        0xdc, 0xf4, 0x20, 0xa0, 0x83, 0x1b, 0xc4, 0x90, 0xc3, 0x61, 0x78, 0x60, 0x0c, 0xd0, 0x63,
        0x1c, 0x30, 0x67, 0x84, 0x43, 0x15, 0xfd, 0xcd, 0xb3, 0xd7, 0x1d, 0xbf, 0x69, 0x38, 0x42,
        0xbb, 0x1b, 0x86, 0x2c, 0x26, 0x94, 0x87, 0x10, 0x22, 0xc8, 0x1d, 0xe9, 0xff, 0x95, 0x01,
    ];
    let mut d = brotli_decompressor::Decompressor::new(Cursor::new(corrupted), 4096);
    let mut sink = Vec::new();
    let err = d.read_to_end(&mut sink).err();
    assert_eq!(err.map(|e| e.kind()), Some(io::ErrorKind::InvalidData),
               "stream overrunning its metablock length must be rejected");
}

// A metablock whose commands emit more than the MLEN declared in its header is
// invalid: RFC 7932 §9.3 says a command that "would cause MLEN to be exceeded"
// means "the stream should be rejected as invalid." The C reference and
// andybalholm/brotli (Go) reject it as BLOCK_LENGTH_2; this decoder previously
// accepted it because the only negative-`meta_block_remaining_len` guard was in
// WriteRingBuffer, which is not reached when the whole output fits in the ring
// buffer before the final flush. This vector was found by differential fuzzing
// against andybalholm.
#[test]
fn test_rejects_metablock_length_overflow() {
    fn unhex(s: &str) -> Vec<u8> {
        (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
    }
    let corrupt = unhex(
        "0b2b01008cd4484d73bb8171bab646fb2203e581794e0cb52237983782d1e0880c00200849a0f3a41ab32b9\
         8909e575ad3dc35383ab3bc757871fff6ab3a7135eeae1f988262c82ca1426ff34692b30a2beadb7a8589f9\
         b5dd93eba74fd1f0b376393efe188bc0d3b812b5c91988a7e5965437750e8c4c2f6d1e9cdfbdfe2876540ed\
         bcb1b222168125320d7593de1a4cc82f2bad61e7e7c6e75e7f8eaf1e35ff7d2663edc7f2803c751396295d1\
         e18fa5e614573576f40f4f2d6eec9fddbe7ccb5658f49bf30b24c02822832fd35adca1c48cfcb2da966e6e6\
         c7665fbe8f2e1fd4f73937adadfbe080dc352d822a5c1ee8ba664175536b4f70d4d2eacef9dde3c7f496690\
         77ebe909e02024813e3000b81200c0488dd434b70b8000286308abc0fb1d453300b81200c0488dd434b70b8\
         0002863088be0b73600b81200c0488dd434b70b80002863088be0b73600b81200c0488dd434b70b80002863\
         088be0b73600b81200c0488dd434b70b80c88fc6428cc0bb0103",
    );

    let mut d = brotli_decompressor::Decompressor::new(Cursor::new(corrupt), 4096);
    let mut sink = Vec::new();
    let err = d.read_to_end(&mut sink).err();
    assert_eq!(
        err.as_ref().map(|e| e.kind()),
        Some(io::ErrorKind::InvalidData),
        "decoder must reject a metablock that overshoots its declared length; got {:?}",
        err.as_ref().map_or_else(|| format!("Ok({} bytes)", sink.len()), |e| format!("{:?}", e)),
    );
}
