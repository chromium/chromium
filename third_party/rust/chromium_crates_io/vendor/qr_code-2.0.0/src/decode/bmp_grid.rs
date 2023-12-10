use crate::decode::decode::codestream_ecc;
use crate::decode::decode::decode_payload;
use crate::decode::decode::read_data;
use crate::decode::decode::read_format;
use crate::decode::BitGrid;
use bmp_monochrome::BmpError;
use std::io::Cursor;

impl BitGrid for &bmp_monochrome::Bmp {
    fn size(&self) -> usize {
        self.width() as usize
    }

    fn bit(&self, y: usize, x: usize) -> bool {
        self.get(y as u16, x as u16)
    }
}

/// Allows to decode the QR coded in a bmp file
pub trait BmpDecode {
    /// Allows to decode the QR coded in a bmp file
    fn decode(&self) -> Result<Vec<u8>, BmpError>;
}

impl BmpDecode for bmp_monochrome::Bmp {
    fn decode(&self) -> Result<Vec<u8>, BmpError> {
        let meta = read_format(&self).unwrap();
        let raw = read_data(&self, &meta);
        let stream = codestream_ecc(&meta, raw).unwrap();
        let mut writer = Cursor::new(vec![]);
        decode_payload(&meta, stream, &mut writer).unwrap();
        Ok(writer.into_inner())
    }
}

#[cfg(test)]
mod tests {
    use crate::decode::decode::{
        codestream_ecc, decode_payload, read_data, read_format, MetaData, Version,
    };
    use crate::decode::BitGrid;
    use bmp_monochrome::Bmp;
    use std::fs::File;
    use std::io::Cursor;

    #[test]
    fn test_decode() {
        let bmp = &Bmp::read(File::open("test_data/test.bmp").unwrap()).unwrap();
        let meta = read_format(&bmp).unwrap();
        let expected = MetaData {
            version: Version(1),
            ecc_level: 0,
            mask: 2,
        };
        assert_eq!(&expected, &meta);

        let raw = read_data(&bmp, &meta);
        let stream = codestream_ecc(&meta, raw).unwrap();
        let mut writer = Cursor::new(vec![]);
        decode_payload(&meta, stream, &mut writer).unwrap();
        let out = String::from_utf8(writer.into_inner()).unwrap();
        assert_eq!("Hello", &out);
    }

    #[test]
    fn test_grid() {
        let expected = r#"
#######..####.#######
#.....#.....#.#.....#
#.###.#.#.#.#.#.###.#
#.###.#.#.#...#.###.#
#.###.#.##..#.#.###.#
#.....#.##.#..#.....#
#######.#.#.#.#######
........#.#..........
#.#####..#.#..#####..
...#....#..####..##.#
..#####..##.#.##.###.
..#..#.##.#####..##..
.###..#####.#..#....#
........#...#..#.#...
#######..#.#.#..#.##.
#.....#.#.#....#####.
#.###.#.#..#.#..#..#.
#.###.#.##.#####.#...
#.###.#.##..#.##..#..
#.....#..#.####.###..
#######.##..#...#..#.
"#;
        let mut chars = expected.chars();
        let bmp = &Bmp::read(File::open("test_data/test.bmp").unwrap()).unwrap();
        for i in 0..bmp.size() {
            for j in 0..bmp.size() {
                let mut char = chars.next().unwrap();
                if char == '\n' {
                    char = chars.next().unwrap();
                }
                assert_eq!(char == '#', bmp.bit(i, j));
            }
        }
    }
}
