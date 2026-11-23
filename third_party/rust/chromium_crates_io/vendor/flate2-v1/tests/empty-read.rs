use std::io::{Read, Write};

#[test]
fn deflate_decoder_empty_read() {
    let original: &[u8] = b"Lorem ipsum dolor sit amet.";
    let mut encoder =
        flate2::write::DeflateEncoder::new(Vec::new(), flate2::Compression::default());
    encoder.write_all(original).unwrap();
    let encoded: Vec<u8> = encoder.finish().unwrap();
    let mut decoder = flate2::read::DeflateDecoder::new(encoded.as_slice());
    assert_eq!(decoder.read(&mut []).unwrap(), 0);
    let mut decoded = Vec::new();
    decoder.read_to_end(&mut decoded).unwrap();
    assert_eq!(decoded.as_slice(), original);
}

#[test]
fn deflate_encoder_empty_read() {
    let original: &[u8] = b"Lorem ipsum dolor sit amet.";
    let mut encoder = flate2::read::DeflateEncoder::new(original, flate2::Compression::default());
    assert_eq!(encoder.read(&mut []).unwrap(), 0);
    let mut encoded = Vec::new();
    encoder.read_to_end(&mut encoded).unwrap();
    let mut decoder = flate2::read::DeflateDecoder::new(encoded.as_slice());
    let mut decoded = Vec::new();
    decoder.read_to_end(&mut decoded).unwrap();
    assert_eq!(decoded.as_slice(), original);
}

#[test]
fn gzip_decoder_empty_read() {
    let original: &[u8] = b"Lorem ipsum dolor sit amet.";
    let mut encoder = flate2::write::GzEncoder::new(Vec::new(), flate2::Compression::default());
    encoder.write_all(original).unwrap();
    let encoded: Vec<u8> = encoder.finish().unwrap();
    let mut decoder = flate2::read::GzDecoder::new(encoded.as_slice());
    assert_eq!(decoder.read(&mut []).unwrap(), 0);
    let mut decoded = Vec::new();
    decoder.read_to_end(&mut decoded).unwrap();
    assert_eq!(decoded.as_slice(), original);
}

fn gzip_encoder(original: &[u8], buffered: bool) -> Box<dyn Read + '_> {
    if buffered {
        Box::new(flate2::bufread::GzEncoder::new(
            original,
            flate2::Compression::default(),
        ))
    } else {
        Box::new(flate2::read::GzEncoder::new(
            original,
            flate2::Compression::default(),
        ))
    }
}

#[test]
fn gzip_encoder_empty_read() {
    for original in [&b""[..], &b"Lorem ipsum dolor sit amet."[..]] {
        for buffered in [false, true] {
            let mut expected = Vec::new();
            gzip_encoder(original, buffered)
                .read_to_end(&mut expected)
                .unwrap();
            for buffer_size in [1, 9, 10, 11, 64] {
                let mut encoder = gzip_encoder(original, buffered);
                let mut buffer = vec![0; buffer_size];
                let mut encoded = Vec::new();
                loop {
                    assert_eq!(encoder.read(&mut []).unwrap(), 0);
                    assert_eq!(encoder.read(&mut []).unwrap(), 0);
                    let count = encoder.read(&mut buffer).unwrap();
                    if count == 0 {
                        break;
                    }
                    encoded.extend_from_slice(&buffer[..count]);
                }
                assert_eq!(encoder.read(&mut []).unwrap(), 0);
                assert_eq!(encoder.read(&mut buffer).unwrap(), 0);
                assert_eq!(encoded, expected);
                let mut decoded = Vec::new();
                flate2::read::GzDecoder::new(encoded.as_slice())
                    .read_to_end(&mut decoded)
                    .unwrap();
                assert_eq!(decoded.as_slice(), original);
            }
        }
    }
}

#[test]
fn gzip_encoder_empty_read_after_header() {
    let original: &[u8] = b"An empty read after the header must not finish the stream.";
    for buffered in [false, true] {
        let mut encoder = gzip_encoder(original, buffered);
        let mut header = [0; 10];
        encoder.read_exact(&mut header).unwrap();
        assert_eq!(encoder.read(&mut []).unwrap(), 0);
        let mut encoded = header.to_vec();
        encoder.read_to_end(&mut encoded).unwrap();
        let mut decoded = Vec::new();
        flate2::read::GzDecoder::new(encoded.as_slice())
            .read_to_end(&mut decoded)
            .unwrap();
        assert_eq!(decoded.as_slice(), original);
    }
}

#[test]
fn zlib_decoder_empty_read() {
    let original: &[u8] = b"Lorem ipsum dolor sit amet.";
    let mut encoder = flate2::write::ZlibEncoder::new(Vec::new(), flate2::Compression::default());
    encoder.write_all(original).unwrap();
    let encoded: Vec<u8> = encoder.finish().unwrap();
    let mut decoder = flate2::read::ZlibDecoder::new(encoded.as_slice());
    assert_eq!(decoder.read(&mut []).unwrap(), 0);
    let mut decoded = Vec::new();
    decoder.read_to_end(&mut decoded).unwrap();
    assert_eq!(decoded.as_slice(), original);
}

#[test]
fn zlib_encoder_empty_read() {
    let original: &[u8] = b"Lorem ipsum dolor sit amet.";
    let mut encoder = flate2::read::ZlibEncoder::new(original, flate2::Compression::default());
    assert_eq!(encoder.read(&mut []).unwrap(), 0);
    let mut encoded = Vec::new();
    encoder.read_to_end(&mut encoded).unwrap();
    let mut decoder = flate2::read::ZlibDecoder::new(encoded.as_slice());
    let mut decoded = Vec::new();
    decoder.read_to_end(&mut decoded).unwrap();
    assert_eq!(decoded.as_slice(), original);
}
