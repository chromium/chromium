use typed_path::TypedPath;

fn main() {
    // Try to be smart to figure out the path (Unix or Windows) automatically
    let path = TypedPath::derive(r"/path/to/file.txt");

    for component in path.components() {
        println!("{}", String::from_utf8_lossy(component.as_bytes()));
    }
}
