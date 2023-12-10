use crate::{Color, QrCode};

impl QrCode {
    /// Render the qr code in a utf-8 string (2x1 pixel per character)
    /// `inverted` toggle the foreground and background color
    pub fn to_string(&self, inverted: bool, border: u8) -> String {
        let mut result = String::new();
        let width = self.width();
        let qr_code = self.clone().into_colors();
        let height = qr_code.len() / width;

        let inverted = if inverted { 0 } else { 4 };
        let blocks = ["█", "▀", "▄", " ", " ", "▄", "▀", "█"];
        let full_block = blocks[inverted];
        let border_blocks: String = (0..border).map(|_| full_block).collect();
        let mut line_full: String = (0..width + border as usize * 2)
            .map(|_| full_block)
            .collect();
        line_full.push('\n');

        for _ in 0..(border + 1) / 2 {
            result.push_str(&line_full);
        }
        for i in (0..height).step_by(2) {
            result.push_str(&border_blocks);
            for j in 0..width {
                let start = i * width + j;
                let val = match (
                    qr_code[start],
                    qr_code.get(start + width).unwrap_or(&Color::Light),
                ) {
                    (Color::Light, Color::Light) => 0,
                    (Color::Light, Color::Dark) => 1,
                    (Color::Dark, Color::Light) => 2,
                    (Color::Dark, Color::Dark) => 3,
                };
                result.push_str(blocks[val + inverted]);
            }
            result.push_str(&border_blocks);
            result.push('\n');
        }
        let odd = if height % 2 == 0 { 1 } else { 0 };
        for _ in 0..(border + odd) / 2 {
            result.push_str(&line_full);
        }
        result.push('\n');
        result
    }

    /// Convert the QRCode to Bmp
    #[cfg(feature = "bmp")]
    #[cfg_attr(docsrs, doc(cfg(feature = "bmp")))]
    pub fn to_bmp(&self) -> bmp_monochrome::Bmp {
        let width = self.width();
        let data = self.to_vec().chunks(width).map(|r| r.to_vec()).collect();
        bmp_monochrome::Bmp::new(data).unwrap()
    }
}
