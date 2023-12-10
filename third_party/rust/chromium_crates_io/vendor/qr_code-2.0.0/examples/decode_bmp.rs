#[cfg(all(feature = "bmp", feature = "decode"))]
fn main() {
    use qr_code::bmp_monochrome::Bmp;
    use qr_code::decode::BmpDecode;
    use std::fs::File;

    let bmp = Bmp::read(File::open("test_data/qr_not_normalized.bmp").unwrap()).unwrap();
    let decoded_vec = bmp.normalize().decode().unwrap();
    let decoded_str = std::str::from_utf8(&decoded_vec).unwrap();
    println!("{}", &decoded_str);
}
