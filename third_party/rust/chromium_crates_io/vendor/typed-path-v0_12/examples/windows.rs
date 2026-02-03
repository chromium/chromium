use typed_path::{Component, WindowsPath};

fn main() {
    let path = WindowsPath::new(r"C:\path\to\file.txt");

    for component in path.components() {
        println!("{}", String::from_utf8_lossy(component.as_bytes()));
    }
}
