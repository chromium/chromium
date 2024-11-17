use rstest::*;
use std::fs::File;
use std::io::Read;
use std::path::PathBuf;

#[rstest]
fn start_with_name(
    #[files("files/**/*.txt")]
    #[by_ref]
    path: &PathBuf,
) {
    let name = path.file_name().unwrap();
    let mut f = File::open(&path).unwrap();
    let mut contents = String::new();
    f.read_to_string(&mut contents).unwrap();

    assert!(contents.starts_with(name.to_str().unwrap()))
}

#[fixture]
fn f() -> u32 {
    42
}

#[rstest]
#[case(42)]
fn test(
    #[by_ref] f: &u32,
    #[case]
    #[by_ref]
    c: &u32,
    #[values(42, 142)]
    #[by_ref]
    v: &u32,
) {
    assert_eq!(f, c);
    assert_eq!(*c, *v % 100);
}
