// Note: This file is only currently used on targets that call out to the code
// in `mod libs_dl_iterate_phdr` (e.g. linux, freebsd, ...); it may be more
// general purpose, but it hasn't been tested elsewhere.

use super::mystd::ffi::OsString;
use super::mystd::fs::File;
use super::mystd::io::Read;
use alloc::string::String;
use alloc::vec::Vec;
use core::str::FromStr;

#[derive(PartialEq, Eq, Debug)]
pub(super) struct MapsEntry {
    /// start (inclusive) and limit (exclusive) of address range.
    address: (usize, usize),
    /// Offset into the file (or "whatever").
    offset: u64,
    /// Usually the file backing the mapping.
    ///
    /// Note: The man page for proc includes a note about "coordination" by
    /// using readelf to see the Offset field in ELF program headers. pnkfelix
    /// is not yet sure if that is intended to be a comment on pathname, or what
    /// form/purpose such coordination is meant to have.
    ///
    /// There are also some pseudo-paths:
    /// "[stack]": The initial process's (aka main thread's) stack.
    /// "[stack:<tid>]": a specific thread's stack. (This was only present for a limited range of Linux verisons; it was determined to be too expensive to provide.)
    /// "[vdso]": Virtual dynamically linked shared object
    /// "[heap]": The process's heap
    ///
    /// The pathname can be blank, which means it is an anonymous mapping
    /// obtained via mmap.
    ///
    /// Newlines in pathname are replaced with an octal escape sequence.
    ///
    /// The pathname may have "(deleted)" appended onto it if the file-backed
    /// path has been deleted.
    ///
    /// Note that modifications like the latter two indicated above imply that
    /// in general the pathname may be ambiguous. (I.e. you cannot tell if the
    /// denoted filename actually ended with the text "(deleted)", or if that
    /// was added by the maps rendering.
    pathname: OsString,
}

pub(super) fn parse_maps() -> Result<Vec<MapsEntry>, &'static str> {
    let mut proc_self_maps =
        File::open("/proc/self/maps").map_err(|_| "Couldn't open /proc/self/maps")?;
    let mut buf = String::new();
    let _bytes_read = proc_self_maps
        .read_to_string(&mut buf)
        .map_err(|_| "Couldn't read /proc/self/maps")?;

    let mut v = Vec::new();
    let mut buf = buf.as_str();
    while let Some(match_idx) = buf.bytes().position(|b| b == b'\n') {
        // Unsafe is unfortunately necessary to get the bounds check removed (for code size).

        // SAFETY: match_idx is the position of the newline, so it must be valid.
        let line = unsafe { buf.get_unchecked(..match_idx) };

        v.push(line.parse()?);

        // SAFETY: match_idx is the position of the newline, so the byte after it must be valid.
        buf = unsafe { buf.get_unchecked((match_idx + 1)..) };
    }

    Ok(v)
}

impl MapsEntry {
    pub(super) fn pathname(&self) -> &OsString {
        &self.pathname
    }

    pub(super) fn ip_matches(&self, ip: usize) -> bool {
        self.address.0 <= ip && ip < self.address.1
    }

    #[cfg(target_os = "android")]
    pub(super) fn offset(&self) -> u64 {
        self.offset
    }
}

impl FromStr for MapsEntry {
    type Err = &'static str;

    // Format: address perms offset dev inode pathname
    // e.g.: "ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]"
    // e.g.: "7f5985f46000-7f5985f48000 rw-p 00039000 103:06 76021795                  /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2"
    // e.g.: "35b1a21000-35b1a22000 rw-p 00000000 00:00 0"
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        // While there are nicer standard library APIs available for this, we aim for minimal code size.

        let mut state = s;

        fn parse_start<'a>(state: &mut &'a str) -> &'a str {
            // Unsafe is unfortunately necessary to get the bounds check removed (for code size).

            let start_idx = state.bytes().position(|b| b != b' ');
            if let Some(start_idx) = start_idx {
                // SAFETY: It comes from position, so it's in bounds.
                //         It must be on a UTF-8 boundary as it's the first byte that isn't ' '.
                *state = unsafe { state.get_unchecked(start_idx..) };
            }
            let match_idx = state.bytes().position(|b| b == b' ');
            match match_idx {
                None => {
                    let result = *state;
                    *state = "";
                    result
                }
                Some(match_idx) => {
                    // SAFETY: match_index comes from .bytes().position() of an ASCII character,
                    //         so it's both in bounds and a UTF-8 boundary
                    let result = unsafe { state.get_unchecked(..match_idx) };
                    // SAFETY: Since match_idx is the ' ', there must be at least the end after it.
                    *state = unsafe { state.get_unchecked((match_idx + 1)..) };
                    result
                }
            }
        }

        fn error(msg: &str) -> &str {
            if cfg!(debug_assertions) {
                msg
            } else {
                "invalid map entry"
            }
        }

        let range_str = parse_start(&mut state);
        if range_str.is_empty() {
            return Err(error("Couldn't find address"));
        }

        let perms_str = parse_start(&mut state);
        if perms_str.is_empty() {
            return Err(error("Couldn't find permissions"));
        }

        let offset_str = parse_start(&mut state);
        if offset_str.is_empty() {
            return Err(error("Couldn't find offset"));
        }

        let dev_str = parse_start(&mut state);
        if dev_str.is_empty() {
            return Err(error("Couldn't find dev"));
        }

        let inode_str = parse_start(&mut state);
        if inode_str.is_empty() {
            return Err(error("Couldn't find inode"));
        }

        // Pathname may be omitted in which case it will be empty
        let pathname_str = state.trim_ascii_start();

        let hex = |s| usize::from_str_radix(s, 16).map_err(|_| error("Couldn't parse hex number"));
        let hex64 = |s| u64::from_str_radix(s, 16).map_err(|_| error("Couldn't parse hex number"));

        let address = if let Some((start, limit)) = range_str.split_once('-') {
            (hex(start)?, hex(limit)?)
        } else {
            return Err(error("Couldn't parse address range"));
        };
        let offset = hex64(offset_str)?;
        let pathname = pathname_str.into();

        Ok(MapsEntry {
            address,
            offset,
            pathname,
        })
    }
}

// Make sure we can parse 64-bit sample output if we're on a 64-bit target.
#[cfg(target_pointer_width = "64")]
#[test]
fn check_maps_entry_parsing_64bit() {
    assert_eq!(
        "ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  \
                [vsyscall]"
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0xffffffffff600000, 0xffffffffff601000),
            offset: 0x00000000,
            pathname: "[vsyscall]".into(),
        }
    );

    assert_eq!(
        "7f5985f46000-7f5985f48000 rw-p 00039000 103:06 76021795                  \
                /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2"
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0x7f5985f46000, 0x7f5985f48000),
            offset: 0x00039000,
            pathname: "/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2".into(),
        }
    );
    assert_eq!(
        "35b1a21000-35b1a22000 rw-p 00000000 00:00 0"
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0x35b1a21000, 0x35b1a22000),
            offset: 0x00000000,
            pathname: Default::default(),
        }
    );
}

// (This output was taken from a 32-bit machine, but will work on any target)
#[test]
fn check_maps_entry_parsing_32bit() {
    /* Example snippet of output:
    08056000-08077000 rw-p 00000000 00:00 0          [heap]
    b7c79000-b7e02000 r--p 00000000 08:01 60662705   /usr/lib/locale/locale-archive
    b7e02000-b7e03000 rw-p 00000000 00:00 0
        */
    assert_eq!(
        "08056000-08077000 rw-p 00000000 00:00 0          \
                [heap]"
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0x08056000, 0x08077000),
            offset: 0x00000000,
            pathname: "[heap]".into(),
        }
    );

    assert_eq!(
        "b7c79000-b7e02000 r--p 00000000 08:01 60662705   \
                /usr/lib/locale/locale-archive"
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0xb7c79000, 0xb7e02000),
            offset: 0x00000000,
            pathname: "/usr/lib/locale/locale-archive".into(),
        }
    );
    assert_eq!(
        "b7e02000-b7e03000 rw-p 00000000 00:00 0"
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0xb7e02000, 0xb7e03000),
            offset: 0x00000000,
            pathname: Default::default(),
        }
    );
    assert_eq!(
        "b7c79000-b7e02000 r--p 00000000 08:01 60662705   \
                /executable/path/with some spaces"
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0xb7c79000, 0xb7e02000),
            offset: 0x00000000,
            pathname: "/executable/path/with some spaces".into(),
        }
    );
    assert_eq!(
        "b7c79000-b7e02000 r--p 00000000 08:01 60662705   \
                /executable/path/with  multiple-continuous    spaces  "
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0xb7c79000, 0xb7e02000),
            offset: 0x00000000,
            pathname: "/executable/path/with  multiple-continuous    spaces  ".into(),
        }
    );
    assert_eq!(
        "  b7c79000-b7e02000  r--p  00000000  08:01  60662705   \
                /executable/path/starts-with-spaces"
            .parse::<MapsEntry>()
            .unwrap(),
        MapsEntry {
            address: (0xb7c79000, 0xb7e02000),
            offset: 0x00000000,
            pathname: "/executable/path/starts-with-spaces".into(),
        }
    );
}
