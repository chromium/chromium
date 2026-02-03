use typed_path::Utf8TypedPath;

fn main() {
    // Try to be smart to figure out the path (Unix or Windows) automatically
    let path = Utf8TypedPath::derive(r"/path/to/file.txt");

    for component in path.components() {
        println!("{}", component.as_str());
    }
}
