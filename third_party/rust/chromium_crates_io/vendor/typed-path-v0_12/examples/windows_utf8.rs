use typed_path::{Utf8Component, Utf8WindowsPath};

fn main() {
    let path = Utf8WindowsPath::new(r"C:\path\to\file.txt");

    for component in path.components() {
        println!("{}", component.as_str());
    }
}
