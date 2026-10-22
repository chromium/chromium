//! HelenOS directory entry API
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/c/include/dirent.h>

// The module is named to avoid name collision with the dirent struct, which causes issues with
// wildcard imports in the parent module.

use crate::prelude::*;

s! {
    pub struct dirent {
        pub d_name: [c_char; 256],
    }
}

extern_ty! {
    pub type DIR;
}

extern "C" {
    pub fn opendir(name: *const c_char) -> *mut DIR;
    pub fn readdir(dir: *mut DIR) -> *mut dirent;
    pub fn closedir(dir: *mut DIR) -> c_int;
    pub fn rewinddir(dir: *mut DIR);
}
