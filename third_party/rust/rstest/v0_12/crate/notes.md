You have to write a file parser. The first test is:

```rust
use std::path::{PathBuf, Path};

#[derive(Default, Debug, PartialEq)]
struct Matrix { rows: usize, cols: usize, data: Vec<i32> }

impl<P: AsRef<Path>> From<P> for Matrix { fn from(_: P) -> Self { Default::default() } }

fn create_empty_file() -> PathBuf { "".into() }

#[test]
fn should_parse_empty_file() {
     let empty = create_empty_file();

     let matrix: Matrix = Matrix::from(empty);

     assert_eq!(matrix, Default::default())
}
```

Ok, `create_empty_file()` should do a lot of job and can be reused in 
other tests and fixtures: it's a good candidate to be a fixture. 
Let's rewrite previous example by use `rstest`: we'll use `empty` for 
fixture instead of `create_empty_file()` because when we use as fixture 
will be an object instead an action.

```rust
use std::path::{PathBuf, Path};

use rstest::*;

#[derive(Default, Debug, PartialEq)]
struct Matrix { rows: usize, cols: usize, data: Vec<i32> }
impl<P: AsRef<Path>> From<P> for Matrix { fn from(_: P) -> Self { Default::default() } }

#[fixture]
fn empty() -> PathBuf {
     let path = "empty_file".into();
     std::fs::File::create(&path).expect("Cannot open");
     path
}

#[rstest]
fn should_parse_empty_file(empty: PathBuf) {
     let matrix: Matrix = Matrix::from(empty);

     assert_eq!(matrix, Default::default())
}
```

Now our `Matrix`'s `From` trait is a little bit more generic and can 
take every struct that implement `AsRef<Path>` like `PathBuf` do. So we 
can generalize our test too:

```rust
use std::path::{PathBuf, Path};

use rstest::*;

#[derive(Default, Debug, PartialEq)]
struct Matrix { rows: usize, cols: usize, data: Vec<i32> }

impl<P: AsRef<Path>> From<P> for Matrix { fn from(_: P) -> Self { Default::default() } }

#[rstest]
fn should_parse_empty_file<P>(empty: P)
     where P: AsRef<Path>
{
     let matrix: Matrix = Matrix::from(empty);

     assert_eq!(matrix, Default::default())
}
```

Our test is neat and clear but we have a big issue to fix: we cannot 
leave `"empty_file"` on our disk, we must remove it we are done! 
That is a job for Rust ownership!!

We should just wrap `PathBuf` in out `TempFile` struct and implement 
both `Drop` and `AsRef<Path>` traits. We'll use `Drop` to delete it.

```rust
use std::path::{PathBuf, Path};

use rstest::*;

struct TempFile(PathBuf);

impl Drop for TempFile {fn drop(&mut self) {
         std::fs::remove_file(&self.0).unwrap();
     }
}

impl AsRef<Path> for TempFile {
    fn as_ref(&self) -> &Path {
        &self.0
    }
}

#[fixture]
fn empty() -> TempFile {
     let path = "empty_file".into();
     std::fs::File::create(&path).expect("Cannot open");
     TempFile(path)
}
