pub mod bufread;
pub mod read;
pub mod write;

#[cfg(test)]
mod tests {
    use crate::io;
    use crate::io::{Read, Write};
    use alloc::string::ToString;
    use alloc::vec::Vec;

    use rand::{rng, Rng};

    use crate::zlib::{read, write};
    use crate::Compression;

    #[test]
    fn roundtrip() {
        let mut real = Vec::new();
        let mut w = write::ZlibEncoder::new(Vec::new(), Compression::default());
        let v = crate::random_bytes().take(1024).collect::<Vec<_>>();
        for _ in 0..200 {
            let to_write = &v[..rng().random_range(0..v.len())];
            real.extend(to_write.iter().copied());
            w.write_all(to_write).unwrap();
        }
        let result = w.finish().unwrap();
        let mut r = read::ZlibDecoder::new(&result[..]);
        let mut ret = Vec::new();
        r.read_to_end(&mut ret).unwrap();
        assert_eq!(ret, real);
    }

    #[test]
    fn drop_writes() {
        let mut data = Vec::new();
        write::ZlibEncoder::new(&mut data, Compression::default())
            .write_all(b"foo")
            .unwrap();
        let mut r = read::ZlibDecoder::new(&data[..]);
        let mut ret = Vec::new();
        r.read_to_end(&mut ret).unwrap();
        assert_eq!(ret, b"foo");
    }

    #[test]
    fn total_in() {
        let mut real = Vec::new();
        let mut w = write::ZlibEncoder::new(Vec::new(), Compression::default());
        let v = crate::random_bytes().take(1024).collect::<Vec<_>>();
        for _ in 0..200 {
            let to_write = &v[..rng().random_range(0..v.len())];
            real.extend(to_write.iter().copied());
            w.write_all(to_write).unwrap();
        }
        let mut result = w.finish().unwrap();

        let result_len = result.len();

        for _ in 0..200 {
            result.extend(v.iter().copied());
        }

        let mut r = read::ZlibDecoder::new(&result[..]);
        let mut ret = Vec::new();
        r.read_to_end(&mut ret).unwrap();
        assert_eq!(ret, real);
        assert_eq!(r.total_in(), result_len as u64);
    }

    #[test]
    fn roundtrip2() {
        let v = crate::random_bytes().take(1024 * 1024).collect::<Vec<_>>();
        let mut r = read::ZlibDecoder::new(read::ZlibEncoder::new(&v[..], Compression::default()));
        let mut ret = Vec::new();
        r.read_to_end(&mut ret).unwrap();
        assert_eq!(ret, v);
    }

    #[test]
    fn roundtrip3() {
        let v = crate::random_bytes().take(1024 * 1024).collect::<Vec<_>>();
        let mut w =
            write::ZlibEncoder::new(write::ZlibDecoder::new(Vec::new()), Compression::default());
        w.write_all(&v).unwrap();
        let w = w.finish().unwrap().finish().unwrap();
        assert_eq!(w, v);
    }

    #[test]
    fn reset_decoder() {
        let v = crate::random_bytes().take(1024 * 1024).collect::<Vec<_>>();
        let mut w = write::ZlibEncoder::new(Vec::new(), Compression::default());
        w.write_all(&v).unwrap();
        let data = w.finish().unwrap();

        {
            let (mut a, mut b, mut c) = (Vec::new(), Vec::new(), Vec::new());
            let mut r = read::ZlibDecoder::new(&data[..]);
            r.read_to_end(&mut a).unwrap();
            r.reset(&data);
            r.read_to_end(&mut b).unwrap();

            let mut r = read::ZlibDecoder::new(&data[..]);
            r.read_to_end(&mut c).unwrap();
            assert!(a == b && b == c && c == v);
        }

        {
            let mut w = write::ZlibDecoder::new(Vec::new());
            w.write_all(&data).unwrap();
            let a = w.reset(Vec::new()).unwrap();
            w.write_all(&data).unwrap();
            let b = w.finish().unwrap();

            let mut w = write::ZlibDecoder::new(Vec::new());
            w.write_all(&data).unwrap();
            let c = w.finish().unwrap();
            assert!(a == b && b == c && c == v);
        }
    }

    #[test]
    fn bad_input() {
        // regress tests: previously caused a panic on drop
        let mut out: Vec<u8> = Vec::new();
        let data: Vec<u8> = (0..255).cycle().take(1024).collect();
        let mut w = write::ZlibDecoder::new(&mut out);
        match w.write_all(&data[..]) {
            Ok(_) => panic!("Expected an error to be returned!"),
            Err(e) => assert_eq!(e.kind(), io::ErrorKind::InvalidInput),
        }
    }

    #[test]
    fn rejects_incomplete_stream() {
        let mut encoder = write::ZlibEncoder::new(Vec::new(), Compression::default());
        encoder.write_all(b"hello").unwrap();
        let compressed = encoder.finish().unwrap();

        for end in compressed.len() - 4..compressed.len() {
            let mut decoder = read::ZlibDecoder::new(&compressed[..end]);
            let error = crate::io::copy(&mut decoder, &mut crate::io::sink()).unwrap_err();
            assert_eq!(error.kind(), io::ErrorKind::UnexpectedEof);
            assert_eq!(error.to_string(), "incomplete deflate stream");
        }
    }

    #[test]
    fn write_decoder_rejects_incomplete_stream() {
        let mut encoder = write::ZlibEncoder::new(Vec::new(), Compression::default());
        encoder.write_all(b"hello").unwrap();
        let compressed = encoder.finish().unwrap();

        for end in 0..compressed.len() {
            let truncated = &compressed[..end];

            let mut decoder = write::ZlibDecoder::new(Vec::new());
            decoder.write_all(truncated).unwrap();
            assert_incomplete(decoder.finish().unwrap_err());

            let mut decoder = write::ZlibDecoder::new(Vec::new());
            decoder.write_all(truncated).unwrap();
            assert_incomplete(decoder.try_finish().unwrap_err());
            decoder
                .reset(Vec::new())
                .expect("decoder should reset after an incomplete stream");
        }

        let mut decoder = write::ZlibDecoder::new(Vec::new());
        decoder.write_all(&compressed).unwrap();
        decoder.try_finish().unwrap();
        decoder.try_finish().unwrap();
        assert_eq!(
            decoder.finish().unwrap(),
            b"hello",
            "finishing an already finished decoder should not change its output"
        );

        fn assert_incomplete(error: io::Error) {
            assert_eq!(error.kind(), io::ErrorKind::UnexpectedEof);
            assert_eq!(error.to_string(), "incomplete deflate stream");
        }
    }

    #[test]
    fn write_decoder_reset_drains_pending_output() {
        let input = vec![b'a'; 32 * 1024 + 1];
        let mut encoder = write::ZlibEncoder::new(Vec::new(), Compression::default());
        encoder.write_all(&input).unwrap();

        let mut decoder = write::ZlibDecoder::new(Vec::new());
        decoder.write_all(&encoder.finish().unwrap()).unwrap();
        let output = decoder
            .reset(Vec::new())
            .expect("decoder should reset after draining pending output");

        assert!(
            output == input,
            "reset should preserve all decoder output byte-for-byte"
        );
    }

    #[test]
    fn qc_reader() {
        ::quickcheck::quickcheck(test as fn(_) -> _);

        fn test(v: Vec<u8>) -> bool {
            let mut r =
                read::ZlibDecoder::new(read::ZlibEncoder::new(&v[..], Compression::default()));
            let mut v2 = Vec::new();
            r.read_to_end(&mut v2).unwrap();
            v == v2
        }
    }

    #[test]
    fn qc_writer() {
        ::quickcheck::quickcheck(test as fn(_) -> _);

        fn test(v: Vec<u8>) -> bool {
            let mut w = write::ZlibEncoder::new(
                write::ZlibDecoder::new(Vec::new()),
                Compression::default(),
            );
            w.write_all(&v).unwrap();
            v == w.finish().unwrap().finish().unwrap()
        }
    }
}
