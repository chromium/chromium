use rstest::*;
use std::fs::File;
use std::io::Read;
use std::path::PathBuf;

#[rstest]
fn start_with_name(
    #[files("files/**/*.txt")]
    #[exclude("exclude")]
    #[files("../files_test_sub_folder/**/*.txt")]
    path: PathBuf,
) {
    let name = path.file_name().unwrap();
    let mut f = File::open(&path).unwrap();
    let mut contents = String::new();
    f.read_to_string(&mut contents).unwrap();

    assert!(contents.starts_with(name.to_str().unwrap()))
}

#[rstest]
fn start_with_name_with_include(
    #[files("files/**/*.txt")]
    #[exclude("exclude")]
    #[include_dot_files]
    path: PathBuf,
) {
    let name = path.file_name().unwrap();
    let mut f = File::open(&path).unwrap();
    let mut contents = String::new();
    f.read_to_string(&mut contents).unwrap();

    assert!(contents.starts_with(name.to_str().unwrap()))
}

mod module {
    #[rstest::rstest]
    fn pathbuf_need_not_be_in_scope(
        #[files("files/**/*.txt")]
        #[exclude("exclude")]
        #[include_dot_files]
        path: std::path::PathBuf,
    ) {
    }
}
