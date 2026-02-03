use typed_path::{Component, UnixPath};

fn main() {
    let path = UnixPath::new(r"/path/to/file.txt");

    for component in path.components() {
        println!("{}", String::from_utf8_lossy(component.as_bytes()));
    }
}
