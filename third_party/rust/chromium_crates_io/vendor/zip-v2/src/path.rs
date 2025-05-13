//! Path manipulation utilities

use std::{
    ffi::OsStr,
    path::{Component, Path},
};

/// Simplify a path by removing the prefix and parent directories and only return normal components
pub(crate) fn simplified_components(input: &Path) -> Option<Vec<&OsStr>> {
    let mut out = Vec::new();
    for component in input.components() {
        match component {
            Component::Prefix(_) | Component::RootDir => return None,
            Component::ParentDir => {
                out.pop()?;
            }
            Component::Normal(_) => out.push(component.as_os_str()),
            Component::CurDir => (),
        }
    }
    Some(out)
}
