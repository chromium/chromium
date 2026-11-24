//! Packed, unpremultiplied BGRA colors shared by palette and painting APIs.

/// An unpremultiplied sRGB color, packed as blue/green/red/alpha from the
/// most significant byte to the least significant byte.
pub type hr_color_t = u32;

/// Returns the blue channel of a packed color.
#[no_mangle]
pub extern "C" fn hr_color_get_blue(color: hr_color_t) -> u8 {
    (color >> 24) as u8
}

/// Returns the green channel of a packed color.
#[no_mangle]
pub extern "C" fn hr_color_get_green(color: hr_color_t) -> u8 {
    (color >> 16) as u8
}

/// Returns the red channel of a packed color.
#[no_mangle]
pub extern "C" fn hr_color_get_red(color: hr_color_t) -> u8 {
    (color >> 8) as u8
}

/// Returns the alpha channel of a packed color.
#[no_mangle]
pub extern "C" fn hr_color_get_alpha(color: hr_color_t) -> u8 {
    color as u8
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn channels_follow_harfbuzz_packing_without_premultiplication() {
        let color: hr_color_t = 0x1278_A403;
        assert_eq!(hr_color_get_blue(color), 0x12);
        assert_eq!(hr_color_get_green(color), 0x78);
        assert_eq!(hr_color_get_red(color), 0xA4);
        assert_eq!(hr_color_get_alpha(color), 3);
    }
}
