use typed_path::{Utf8Component, Utf8UnixPath};

fn main() {
    let path = Utf8UnixPath::new("/path/to/file.txt");

    for component in path.components() {
        println!("{}", component.as_str());
    }
}
