fn main() {
    let qr_code = qr_code::QrCode::new(b"Hello").unwrap();
    println!("{}", qr_code.to_string(true, 3));
}
