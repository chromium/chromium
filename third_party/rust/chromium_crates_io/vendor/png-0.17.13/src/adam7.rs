//! Utility functions
use std::iter::StepBy;
use std::ops::Range;

/// This iterator iterates over the different passes of an image Adam7 encoded
/// PNG image
/// The pattern is:
///     16462646
///     77777777
///     56565656
///     77777777
///     36463646
///     77777777
///     56565656
///     77777777
///
#[derive(Clone)]
pub(crate) struct Adam7Iterator {
    line: u32,
    lines: u32,
    line_width: u32,
    current_pass: u8,
    width: u32,
    height: u32,
}

impl Adam7Iterator {
    pub fn new(width: u32, height: u32) -> Adam7Iterator {
        let mut this = Adam7Iterator {
            line: 0,
            lines: 0,
            line_width: 0,
            current_pass: 1,
            width,
            height,
        };
        this.init_pass();
        this
    }

    /// Calculates the bounds of the current pass
    fn init_pass(&mut self) {
        let w = f64::from(self.width);
        let h = f64::from(self.height);
        let (line_width, lines) = match self.current_pass {
            1 => (w / 8.0, h / 8.0),
            2 => ((w - 4.0) / 8.0, h / 8.0),
            3 => (w / 4.0, (h - 4.0) / 8.0),
            4 => ((w - 2.0) / 4.0, h / 4.0),
            5 => (w / 2.0, (h - 2.0) / 4.0),
            6 => ((w - 1.0) / 2.0, h / 2.0),
            7 => (w, (h - 1.0) / 2.0),
            _ => unreachable!(),
        };
        self.line_width = line_width.ceil() as u32;
        self.lines = lines.ceil() as u32;
        self.line = 0;
    }

    /// The current pass#.
    pub fn current_pass(&self) -> u8 {
        self.current_pass
    }
}

/// Iterates over the (passes, lines, widths)
impl Iterator for Adam7Iterator {
    type Item = (u8, u32, u32);
    fn next(&mut self) -> Option<Self::Item> {
        if self.line < self.lines && self.line_width > 0 {
            let this_line = self.line;
            self.line += 1;
            Some((self.current_pass, this_line, self.line_width))
        } else if self.current_pass < 7 {
            self.current_pass += 1;
            self.init_pass();
            self.next()
        } else {
            None
        }
    }
}

fn subbyte_pixels(scanline: &[u8], bits_pp: usize) -> impl Iterator<Item = u8> + '_ {
    (0..scanline.len() * 8)
        .step_by(bits_pp)
        .map(move |bit_idx| {
            let byte_idx = bit_idx / 8;

            // sub-byte samples start in the high-order bits
            let rem = 8 - bit_idx % 8 - bits_pp;

            match bits_pp {
                // evenly divides bytes
                1 => (scanline[byte_idx] >> rem) & 1,
                2 => (scanline[byte_idx] >> rem) & 3,
                4 => (scanline[byte_idx] >> rem) & 15,
                _ => unreachable!(),
            }
        })
}

/// Given pass, image width, and line number, produce an iterator of bit positions of pixels to copy
/// from the input scanline to the image buffer.
fn expand_adam7_bits(
    pass: u8,
    width: usize,
    line_no: usize,
    bits_pp: usize,
) -> StepBy<Range<usize>> {
    let (line_mul, line_off, samp_mul, samp_off) = match pass {
        1 => (8, 0, 8, 0),
        2 => (8, 0, 8, 4),
        3 => (8, 4, 4, 0),
        4 => (4, 0, 4, 2),
        5 => (4, 2, 2, 0),
        6 => (2, 0, 2, 1),
        7 => (2, 1, 1, 0),
        _ => panic!("Adam7 pass out of range: {}", pass),
    };

    // the equivalent line number in progressive scan
    let prog_line = line_mul * line_no + line_off;
    // line width is rounded up to the next byte
    let line_width = (width * bits_pp + 7) & !7;
    let line_start = prog_line * line_width;
    let start = line_start + (samp_off * bits_pp);
    let stop = line_start + (width * bits_pp);

    (start..stop).step_by(bits_pp * samp_mul)
}

/// Expands an Adam 7 pass
pub fn expand_pass(
    img: &mut [u8],
    width: u32,
    scanline: &[u8],
    pass: u8,
    line_no: u32,
    bits_pp: u8,
) {
    let width = width as usize;
    let line_no = line_no as usize;
    let bits_pp = bits_pp as usize;

    // pass is out of range but don't blow up
    if pass == 0 || pass > 7 {
        return;
    }

    let bit_indices = expand_adam7_bits(pass, width, line_no, bits_pp);

    if bits_pp < 8 {
        for (pos, px) in bit_indices.zip(subbyte_pixels(scanline, bits_pp)) {
            let rem = 8 - pos % 8 - bits_pp;
            img[pos / 8] |= px << rem as u8;
        }
    } else {
        let bytes_pp = bits_pp / 8;

        for (bitpos, px) in bit_indices.zip(scanline.chunks(bytes_pp)) {
            for (offset, val) in px.iter().enumerate() {
                img[bitpos / 8 + offset] = *val;
            }
        }
    }
}

#[test]
fn test_adam7() {
    /*
        1646
        7777
        5656
        7777
    */
    let it = Adam7Iterator::new(4, 4);
    let passes: Vec<_> = it.collect();
    assert_eq!(
        &*passes,
        &[
            (1, 0, 1),
            (4, 0, 1),
            (5, 0, 2),
            (6, 0, 2),
            (6, 1, 2),
            (7, 0, 4),
            (7, 1, 4)
        ]
    );
}

#[test]
fn test_subbyte_pixels() {
    let scanline = &[0b10101010, 0b10101010];

    let pixels = subbyte_pixels(scanline, 1).collect::<Vec<_>>();
    assert_eq!(pixels.len(), 16);
    assert_eq!(pixels, [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0]);
}

#[test]
fn test_expand_adam7_bits() {
    let width = 32;
    let bits_pp = 1;

    let expected = |offset: usize, step: usize, count: usize| {
        (0..count)
            .map(move |i| step * i + offset)
            .collect::<Vec<_>>()
    };

    for line_no in 0..8 {
        let start = 8 * line_no * width;

        assert_eq!(
            expand_adam7_bits(1, width, line_no, bits_pp).collect::<Vec<_>>(),
            expected(start, 8, 4)
        );

        let start = start + 4;

        assert_eq!(
            expand_adam7_bits(2, width, line_no, bits_pp).collect::<Vec<_>>(),
            expected(start, 8, 4)
        );

        let start = (8 * line_no + 4) * width;

        assert_eq!(
            expand_adam7_bits(3, width, line_no, bits_pp).collect::<Vec<_>>(),
            expected(start, 4, 8)
        );
    }

    for line_no in 0..16 {
        let start = 4 * line_no * width + 2;

        assert_eq!(
            expand_adam7_bits(4, width, line_no, bits_pp).collect::<Vec<_>>(),
            expected(start, 4, 8)
        );

        let start = (4 * line_no + 2) * width;

        assert_eq!(
            expand_adam7_bits(5, width, line_no, bits_pp).collect::<Vec<_>>(),
            expected(start, 2, 16)
        )
    }

    for line_no in 0..32 {
        let start = 2 * line_no * width + 1;

        assert_eq!(
            expand_adam7_bits(6, width, line_no, bits_pp).collect::<Vec<_>>(),
            expected(start, 2, 16),
            "line_no: {}",
            line_no
        );

        let start = (2 * line_no + 1) * width;

        assert_eq!(
            expand_adam7_bits(7, width, line_no, bits_pp).collect::<Vec<_>>(),
            expected(start, 1, 32)
        );
    }
}

#[test]
fn test_expand_pass_subbyte() {
    let mut img = [0u8; 8];
    let width = 8;
    let bits_pp = 1;

    expand_pass(&mut img, width, &[0b10000000], 1, 0, bits_pp);
    assert_eq!(img, [0b10000000u8, 0, 0, 0, 0, 0, 0, 0]);

    expand_pass(&mut img, width, &[0b10000000], 2, 0, bits_pp);
    assert_eq!(img, [0b10001000u8, 0, 0, 0, 0, 0, 0, 0]);

    expand_pass(&mut img, width, &[0b11000000], 3, 0, bits_pp);
    assert_eq!(img, [0b10001000u8, 0, 0, 0, 0b10001000, 0, 0, 0]);

    expand_pass(&mut img, width, &[0b11000000], 4, 0, bits_pp);
    assert_eq!(img, [0b10101010u8, 0, 0, 0, 0b10001000, 0, 0, 0]);

    expand_pass(&mut img, width, &[0b11000000], 4, 1, bits_pp);
    assert_eq!(img, [0b10101010u8, 0, 0, 0, 0b10101010, 0, 0, 0]);

    expand_pass(&mut img, width, &[0b11110000], 5, 0, bits_pp);
    assert_eq!(img, [0b10101010u8, 0, 0b10101010, 0, 0b10101010, 0, 0, 0]);

    expand_pass(&mut img, width, &[0b11110000], 5, 1, bits_pp);
    assert_eq!(
        img,
        [0b10101010u8, 0, 0b10101010, 0, 0b10101010, 0, 0b10101010, 0]
    );

    expand_pass(&mut img, width, &[0b11110000], 6, 0, bits_pp);
    assert_eq!(
        img,
        [0b11111111u8, 0, 0b10101010, 0, 0b10101010, 0, 0b10101010, 0]
    );

    expand_pass(&mut img, width, &[0b11110000], 6, 1, bits_pp);
    assert_eq!(
        img,
        [0b11111111u8, 0, 0b11111111, 0, 0b10101010, 0, 0b10101010, 0]
    );

    expand_pass(&mut img, width, &[0b11110000], 6, 2, bits_pp);
    assert_eq!(
        img,
        [0b11111111u8, 0, 0b11111111, 0, 0b11111111, 0, 0b10101010, 0]
    );

    expand_pass(&mut img, width, &[0b11110000], 6, 3, bits_pp);
    assert_eq!(
        [0b11111111u8, 0, 0b11111111, 0, 0b11111111, 0, 0b11111111, 0],
        img
    );

    expand_pass(&mut img, width, &[0b11111111], 7, 0, bits_pp);
    assert_eq!(
        [
            0b11111111u8,
            0b11111111,
            0b11111111,
            0,
            0b11111111,
            0,
            0b11111111,
            0
        ],
        img
    );

    expand_pass(&mut img, width, &[0b11111111], 7, 1, bits_pp);
    assert_eq!(
        [
            0b11111111u8,
            0b11111111,
            0b11111111,
            0b11111111,
            0b11111111,
            0,
            0b11111111,
            0
        ],
        img
    );

    expand_pass(&mut img, width, &[0b11111111], 7, 2, bits_pp);
    assert_eq!(
        [
            0b11111111u8,
            0b11111111,
            0b11111111,
            0b11111111,
            0b11111111,
            0b11111111,
            0b11111111,
            0
        ],
        img
    );

    expand_pass(&mut img, width, &[0b11111111], 7, 3, bits_pp);
    assert_eq!(
        [
            0b11111111u8,
            0b11111111,
            0b11111111,
            0b11111111,
            0b11111111,
            0b11111111,
            0b11111111,
            0b11111111
        ],
        img
    );
}
