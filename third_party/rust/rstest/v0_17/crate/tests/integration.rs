use rstest_test::{sanitize_name, testname, Project};

/// Rstest integration tests
mod rstest;

/// Fixture's integration tests
mod fixture;

use lazy_static::lazy_static;

use std::path::{Path, PathBuf};
use temp_testdir::TempDir;

lazy_static! {
    static ref ROOT_DIR: TempDir = TempDir::default().permanent();
    static ref ROOT_PROJECT: Project = Project::new(ROOT_DIR.as_ref());
}

pub fn base_prj() -> Project {
    let prj_name = sanitize_name(testname());

    ROOT_PROJECT.subproject(&prj_name)
}

pub fn prj() -> Project {
    let prj_name = sanitize_name(testname());

    let prj = ROOT_PROJECT.subproject(&prj_name);
    prj.add_local_dependency("rstest");
    prj
}

pub fn resources<O: AsRef<Path>>(name: O) -> PathBuf {
    Path::new("tests").join("resources").join(name)
}
