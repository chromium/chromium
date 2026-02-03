use typed_path::PlatformPath;

fn main() {
    // You can create the path like normal, but it is a distinct encoding from Unix/Windows
    let path = PlatformPath::new("some/path");

    // The path will still behave like normal and even report its underlying encoding
    assert_eq!(path.has_unix_encoding(), cfg!(unix));
    assert_eq!(path.has_windows_encoding(), cfg!(windows));

    // It can still be converted into specific platform paths
    let _ = path.with_unix_encoding();
    let _ = path.with_windows_encoding();
}
