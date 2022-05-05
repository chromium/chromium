// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    fs::File,
    io::Write,
    path::{Path, PathBuf},
};

/// Type which knows how to write a .d file. All outputs depend on all
/// dependencies.
pub(crate) struct Depfile {
    file: File,
    outputs: Vec<String>,
    dependencies: Vec<String>,
    depfile_dir: PathBuf,
}

impl Depfile {
    pub(crate) fn new(depfile: &Path) -> std::io::Result<Self> {
        let file = File::create(depfile)?;
        Ok(Self {
            file,
            outputs: Vec::new(),
            dependencies: Vec::new(),
            depfile_dir: depfile.parent().unwrap().to_path_buf(),
        })
    }

    pub(crate) fn add_dependency(&mut self, dependency: &Path) {
        self.dependencies.push(self.relativize(dependency))
    }

    pub(crate) fn add_output(&mut self, output: &Path) {
        self.outputs.push(self.relativize(output))
    }

    pub(crate) fn write(&mut self) -> std::io::Result<()> {
        let dependency_list = self.dependencies.join(" \\\n  ");
        for output in &self.outputs {
            self.file
                .write_all(format!("{}: {}\n\n", output, dependency_list).as_bytes())?
        }
        Ok(())
    }

    /// Return a string giving a relative path from the depfile.
    fn relativize(&self, path: &Path) -> String {
        pathdiff::diff_paths(path, &self.depfile_dir)
            .expect("Unable to make a relative path from the depfile's directory to the dependency")
            .to_str()
            .expect("Unable to represent the file path in a UTF8 encoding")
            .into()
    }
}

#[cfg(test)]
mod tests {
    use std::{fs::File, io::Read};

    use tempdir::TempDir;

    use super::Depfile;

    #[test]
    fn test_simple_depfile() {
        let tmp_dir = TempDir::new("depfile-test").unwrap();
        let f = tmp_dir.path().join("depfile.d");
        let mut df = Depfile::new(&f).unwrap();
        df.add_output(&tmp_dir.path().join("a/b"));
        df.add_dependency(&tmp_dir.path().join("c/d"));
        df.add_dependency(&tmp_dir.path().join("e/f"));
        df.write().unwrap();

        let mut f = File::open(&f).unwrap();
        let mut contents = String::new();
        f.read_to_string(&mut contents).unwrap();
        assert_eq!(contents, "a/b: c/d \\\n  e/f\n\n");
    }

    #[test]
    fn test_multiple_outputs() {
        let tmp_dir = TempDir::new("depfile-test").unwrap();
        let f = tmp_dir.path().join("depfile.d");
        let mut df = Depfile::new(&f).unwrap();
        df.add_output(&tmp_dir.path().join("a/b"));
        df.add_output(&tmp_dir.path().join("z"));
        df.add_dependency(&tmp_dir.path().join("c/d"));
        df.add_dependency(&tmp_dir.path().join("e/f"));
        df.write().unwrap();

        let mut f = File::open(&f).unwrap();
        let mut contents = String::new();
        f.read_to_string(&mut contents).unwrap();
        assert_eq!(contents, "a/b: c/d \\\n  e/f\n\nz: c/d \\\n  e/f\n\n");
    }
}
