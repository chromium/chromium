use std::env::VarError;
use std::process::{
    Command,
    Output,
};
use std::sync::atomic::AtomicBool;
use std::sync::atomic::Ordering::Relaxed;
use std::{
    env,
    fmt,
    str,
};

/// All possible cfgs that may be set. Used for check-cfg as well as self checks.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
enum Cfg {
    LibcDenyWarnings,
    /// MSRV-friendly version of `target_abi = "elfv2"`.
    LibcElfv2,
    /// MSRV-friendly version of `target_abi = "pauthtest"`.
    LibcPauthtest,

    EmscriptenOldStatAbi,

    /// Should be enabled by users if esp-idf (>=6.0) is build with picolibc instead of newlib. Not
    /// set by `libc` itself.
    EspidfPicolibc,
    /// Not set by `libc` itself.
    EspidfTime32,

    Freebsd10,
    Freebsd11,
    Freebsd12,
    Freebsd13,
    Freebsd14,
    Freebsd15,

    VxworksLt25_09,

    /// Corresponds to `__USE_TIME_BITS64` in UAPI. Implies Linux and 64-bit `time_t`, but is set
    /// on both 32-bit and 64-bit platforms.
    LinuxTimeBits64,
    /// Corresponds to `_FILE_OFFSET_BITS=64` in glibc. Implies 32-bit GNU target.
    GnuFileOffsetBits64,
    /// Corresponds to `_TIME_BITS=64` in glibc. Also used in x86 Windows with GNU to expose a
    /// 64-bit `time_t`. Implies 32-bit GNU target and, on platforms other than Windows, 64-bit
    /// `off_t`.
    GnuTimeBits64,
    /// Musl 1.2+. Implies `target_env = "musl"`
    MuslV1_2,
    /// musl v1.2.0+ && 32-bit: time_t is i64, struct layouts change. Implies `musl_v1_2` and
    /// 32-bit arch.
    Musl32Time64,
    /// Corresponds to `_REDIR_TIME64` in musl, i.e. 32-bit platforms that existed prior to the
    /// transition to 64-bit `time_t` and thus need `__*_time64` redirects. Implies `musl_v1_2`
    /// and 32-bit arch.
    MuslRedirTime64,
    /// Corresponds to the `UCLIBC_USE_TIME64` config / `__UCLIBC_USE_TIME64__` define. Indicates
    /// that `*time64` syscalls should be used. Implies 32-bit.
    Uclibc32Time64,
}

const ALLOWED_CFGS: &[Cfg] = &[
    Cfg::LibcDenyWarnings,
    Cfg::EmscriptenOldStatAbi,
    Cfg::EspidfPicolibc,
    Cfg::EspidfTime32,
    Cfg::Freebsd10,
    Cfg::Freebsd11,
    Cfg::Freebsd12,
    Cfg::Freebsd13,
    Cfg::Freebsd14,
    Cfg::Freebsd15,
    Cfg::LibcElfv2,
    Cfg::VxworksLt25_09,
    Cfg::LibcPauthtest,
    Cfg::GnuFileOffsetBits64,
    Cfg::GnuTimeBits64,
    Cfg::LinuxTimeBits64,
    Cfg::MuslV1_2,
    Cfg::Musl32Time64,
    Cfg::MuslRedirTime64,
    Cfg::Uclibc32Time64,
];

impl fmt::Display for Cfg {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Cfg::LibcDenyWarnings => "libc_deny_warnings",
            Cfg::EmscriptenOldStatAbi => "emscripten_old_stat_abi",
            Cfg::EspidfPicolibc => "espidf_picolibc",
            Cfg::EspidfTime32 => "espidf_time32",
            Cfg::Freebsd10 => "freebsd10",
            Cfg::Freebsd11 => "freebsd11",
            Cfg::Freebsd12 => "freebsd12",
            Cfg::Freebsd13 => "freebsd13",
            Cfg::Freebsd14 => "freebsd14",
            Cfg::Freebsd15 => "freebsd15",
            Cfg::LibcElfv2 => "libc_elfv2",
            Cfg::VxworksLt25_09 => "vxworks_lt_25_09",
            Cfg::LibcPauthtest => "libc_pauthtest",
            Cfg::GnuFileOffsetBits64 => "gnu_file_offset_bits64",
            Cfg::GnuTimeBits64 => "gnu_time_bits64",
            Cfg::LinuxTimeBits64 => "linux_time_bits64",
            Cfg::MuslV1_2 => "musl_v1_2",
            Cfg::Musl32Time64 => "musl32_time64",
            Cfg::MuslRedirTime64 => "musl_redir_time64",
            Cfg::Uclibc32Time64 => "uclibc32_time64",
        })
    }
}

// Extra values to allow for check-cfg.
const CHECK_CFG_EXTRA: &[(&str, &[&str])] = &[
    (
        "target_os",
        &[
            "switch", "aix", "ohos", "hurd", "rtems", "visionos", "nuttx", "cygwin", "qurt", "qnx",
            "helenos",
        ],
    ),
    (
        "target_env",
        &["illumos", "wasi", "aix", "ohos", "nto71_iosock"],
    ),
    (
        "target_arch",
        &["loongarch64", "mips32r6", "mips64r6", "csky"],
    ),
];

/// Read from env, print more debug output via `cargo:warning` if set.
static VERBOSE_BUILD: AtomicBool = AtomicBool::new(false);

/// Print info via warnings if `LIBC_BUILD_VERBOSE` is set.
macro_rules! info {
    ($($tt:tt)+) => {
        if VERBOSE_BUILD.load(Relaxed) {
            println!("cargo:warning=info: {}", format_args!($($tt)*));
        }
    }
}

fn main() {
    // Avoid unnecessary re-building.
    println!("cargo:rerun-if-changed=build.rs");

    println!("cargo:rerun-if-env-changed=LIBC_BUILD_VERBOSE");
    if env_flag("LIBC_BUILD_VERBOSE") {
        VERBOSE_BUILD.store(true, Relaxed);
    }

    let mut cfgs = Vec::new();

    let (rustc_minor_ver, _is_nightly) = rustc_minor_nightly();
    let libc_ci = env_flag("LIBC_CI");
    let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let target_ptr_width = env::var("CARGO_CFG_TARGET_POINTER_WIDTH").unwrap_or_default();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    let target_abi = env::var("CARGO_CFG_TARGET_ABI").unwrap_or_default();

    // FIXME(msrv): Once the MSRV is 1.78, use `cfg(target_abi)` directly instead
    // of translating its values. `target_abi` cannot be used directly in cfg
    // expressions on the current MSRV.
    if target_abi == "elfv2" {
        cfgs.push(Cfg::LibcElfv2);
    }
    if target_abi == "pauthtest" {
        cfgs.push(Cfg::LibcPauthtest);
    }

    // FIXME: this can be removed in 1-2 releases
    println!("cargo:rerun-if-env-changed=RUST_LIBC_UNSTABLE_FREEBSD_VERSION");
    if env::var("RUST_LIBC_UNSTABLE_FREEBSD_VERSION").is_ok() {
        println!(
            "cargo:warning=RUST_LIBC_UNSTABLE_FREEBSD_VERSION has been removed; set \
            the cfg libc_unstable_freebsd_version via RUSTFLAGS instead"
        );
    }

    if target_os == "freebsd" {
        // Determine the default FreeBSD version.
        let freebsd_default = if env::var("CARGO_FEATURE_RUSTC_DEP_OF_STD").is_ok() {
            // The ABI of libc used by std is backward compatible with FreeBSD 12.
            12
        } else {
            // The ABI of libc from crates.io is backward compatible with FreeBSD 12.
            12
        };
        // On CI, we detect the actual FreeBSD version and match its ABI exactly,
        // running tests to ensure that the ABI is correct.
        // Allow overriding the default version for testing
        let which_freebsd = if let Ok(version) = env::var("CARGO_CFG_LIBC_UNSTABLE_FREEBSD_VERSION")
        {
            let vers = version.parse().unwrap();
            println!("cargo:warning=setting FreeBSD version to {vers}");
            vers
        } else if libc_ci {
            which_freebsd().unwrap_or(freebsd_default)
        } else {
            freebsd_default
        };

        match which_freebsd {
            x if x < 10 => panic!("FreeBSD older than 10 is not supported"),
            10 => cfgs.push(Cfg::Freebsd10),
            11 => cfgs.push(Cfg::Freebsd11),
            12 => cfgs.push(Cfg::Freebsd12),
            13 => cfgs.push(Cfg::Freebsd13),
            14 => cfgs.push(Cfg::Freebsd14),
            _ => cfgs.push(Cfg::Freebsd15),
        }
    }

    if target_os == "emscripten" {
        match emcc_version_code() {
            Some(v) if (v < 30142) => cfgs.push(Cfg::EmscriptenOldStatAbi),
            // Non-Emscripten or version >= 3.1.42.
            _ => (),
        }
    }

    if target_os == "vxworks" {
        match vxworks_version_code() {
            Some(v) if (v < (25, 9)) => cfgs.push(Cfg::VxworksLt25_09),
            // VxWorks version >= 25.09
            _ => (),
        }
    }

    let mut musl_v1_2_env = env_flag("CARGO_CFG_LIBC_UNSTABLE_MUSL_V1_2");
    if let Ok(old_musl_v1_2_3_env) = env::var("CARGO_CFG_LIBC_UNSTABLE_MUSL_V1_2_3") {
        if !musl_v1_2_env {
            println!(
                "cargo:warning=`--cfg=libc_unstable_musl_v1_2_3` will be removed; \
                set `--cfg=libc_unstable_musl_v1_2`instead"
            );
        }
        musl_v1_2_env |= old_musl_v1_2_3_env != "0";
    }
    if let Ok(old_musl_v1_2_3_env) = env::var("RUST_LIBC_UNSTABLE_MUSL_V1_2_3") {
        if !musl_v1_2_env {
            println!(
                "cargo:warning=RUST_LIBC_UNSTABLE_MUSL_V1_2_3 will be removed; \
                set `--cfg=libc_unstable_musl_v1_2` via RUSTFLAGS instead"
            );
        }
        musl_v1_2_env |= old_musl_v1_2_3_env != "0";
    }

    // Targets that only exist with recent musl. 32-bit targets not in this list need
    // `_REDIR_TIME64` for 64-bit `time_t`.
    let only_v1_2_on_musl = target_arch == "loongarch64"
        || target_arch == "hexagon"
        || target_arch == "riscv32"
        || target_env == "ohos"
        || target_abi == "pauthtest";

    // Single entry-point to enable `time64` on supported platforms.
    let time64 = env_flag("CARGO_CFG_LIBC_UNSTABLE_TIME64");

    // OpenHarmony uses a fork of the musl libc
    let musl = target_env == "musl" || target_env == "ohos";
    let musl_v1_2 = musl && (musl_v1_2_env || only_v1_2_on_musl || time64);

    if musl_v1_2 {
        cfgs.push(Cfg::MuslV1_2);
        cfgs.push(Cfg::LinuxTimeBits64);
        if target_ptr_width == "32" {
            cfgs.push(Cfg::Musl32Time64);
            if !only_v1_2_on_musl {
                // Older 32-bit arches need the redirects
                cfgs.push(Cfg::MuslRedirTime64);
            }
        }
    }

    let mut gnu_tb_env = env::var("CARGO_CFG_LIBC_UNSTABLE_GNU_TIME_BITS");

    // FIXME: remove these fallbacks in a few releases
    if let Ok(old_gnu_tb_env) = env::var("RUST_LIBC_UNSTABLE_GNU_TIME_BITS") {
        println!(
            "cargo:warning=RUST_LIBC_UNSTABLE_GNU_TIME_BITS will be removed; \
            set `--cfg=libc_unstable_gnu_time_bits=\"...\"` via RUSTFLAGS instead"
        );
        gnu_tb_env = gnu_tb_env.or(Ok(old_gnu_tb_env));
    }
    if env::var("RUST_LIBC_UNSTABLE_GNU_FILE_OFFSET_BITS").is_ok()
        || env::var("CARGO_CFG_LIBC_UNSTABLE_GNU_FILE_OFFSET_BITS").is_ok()
    {
        println!(
            "cargo:warning=glibc file offset can no longer be set independently of \
            `gnu_time_bits`"
        );
    }

    let gnu32_timebits = match gnu_tb_env.as_deref() {
        Err(_) => "32", // default to 32
        Ok(tb) if tb == "64" => tb,
        Ok(tb) if tb == "32" => tb,
        Ok(_) => {
            panic!("Invalid value for libc_unstable_gnu_time_bits. Must be 32, 64, or unset.")
        }
    };

    // 32-bit arches with 64-bit time_t by default. rv32 is 64-only, `x86_64` covers the x32 arch,
    // `!(linux|windows|hurd)` covers vxworks and future platforms.
    let gnu32_already_time64 = target_arch == "riscv32"
        || target_arch == "x86_64"
        || !matches!(target_os.as_str(), "linux" | "windows" | "hurd");
    let gnu = target_env == "gnu";
    let gnu32_time64 = gnu && target_ptr_width == "32" && (gnu32_timebits == "64" || time64);

    if gnu32_time64 && !gnu32_already_time64 {
        // These configs all set up nonstandard options. They are not needed on platforms like
        // riscv32, where 64-bit `time_t` is the default.
        if target_os == "linux" {
            cfgs.push(Cfg::LinuxTimeBits64);
        }
        cfgs.push(Cfg::GnuFileOffsetBits64);
        cfgs.push(Cfg::GnuTimeBits64);
    }

    let uclibc_time64_env = env_flag("CARGO_CFG_LIBC_UNSTABLE_UCLIBC_TIME64");
    let uclibc = target_env == "uclibc";
    let uclibc32_time64 = uclibc && target_ptr_width == "32" && (uclibc_time64_env || time64);

    if uclibc32_time64 {
        if target_os == "linux" {
            cfgs.push(Cfg::LinuxTimeBits64);
        }
        cfgs.push(Cfg::Uclibc32Time64);
    }

    // `__USE_TIME_BITS64` is set on any platform if `time_t` is 64 bits.
    if target_os == "linux" && target_ptr_width == "64" {
        cfgs.push(Cfg::LinuxTimeBits64);
    }

    // On CI: deny all warnings
    if libc_ci {
        cfgs.push(Cfg::LibcDenyWarnings);
    }

    // Since Rust 1.80, configuration that isn't recognized by default needs to be provided to
    // avoid warnings.
    if rustc_minor_ver >= 80 {
        for cfg in ALLOWED_CFGS {
            println!("cargo:rustc-check-cfg=cfg({cfg})");
        }
        for &(name, values) in CHECK_CFG_EXTRA {
            let values = values.join("\",\"");
            println!("cargo:rustc-check-cfg=cfg({name},values(\"{values}\"))");
        }
    }

    cfgs.sort_unstable();
    cfgs.dedup();
    validate_cfg(&cfgs, &target_env, &target_os, &target_ptr_width);

    for cfg in cfgs {
        set_cfg(cfg);
    }
}

/// Run `rustc --version` and capture the output, adjusting arguments as needed if `clippy-driver`
/// is used instead.
fn rustc_version_cmd(is_clippy_driver: bool) -> Output {
    let rustc = env::var_os("RUSTC").expect("Failed to get rustc version: missing RUSTC env");

    let mut cmd = match env::var_os("RUSTC_WRAPPER") {
        Some(ref wrapper) if wrapper.is_empty() => Command::new(rustc),
        Some(wrapper) => {
            let mut cmd = Command::new(wrapper);
            cmd.arg(rustc);
            if is_clippy_driver {
                cmd.arg("--rustc");
            }

            cmd
        }
        None => Command::new(rustc),
    };

    cmd.arg("--version");

    let output = cmd.output().expect("Failed to get rustc version");

    assert!(
        output.status.success(),
        "failed to run rustc: {}",
        String::from_utf8_lossy(output.stderr.as_slice())
    );

    output
}

/// Return the minor version of `rustc`, as well as a bool indicating whether or not the version
/// is a nightly.
fn rustc_minor_nightly() -> (u32, bool) {
    macro_rules! otry {
        ($e:expr) => {
            match $e {
                Some(e) => e,
                None => panic!("Failed to get rustc version"),
            }
        };
    }

    let mut output = rustc_version_cmd(false);

    if otry!(str::from_utf8(&output.stdout).ok()).starts_with("clippy") {
        output = rustc_version_cmd(true);
    }

    let version = otry!(str::from_utf8(&output.stdout).ok());

    let mut pieces = version.split('.');

    assert_eq!(
        pieces.next(),
        Some("rustc 1"),
        "Failed to get rustc version"
    );

    let minor = pieces.next();

    // If `rustc` was built from a tarball, its version string
    // will have neither a git hash nor a commit date
    // (e.g. "rustc 1.39.0"). Treat this case as non-nightly,
    // since a nightly build should either come from CI
    // or a git checkout
    let nightly_raw = otry!(pieces.next()).split('-').nth(1);
    let nightly = nightly_raw.map_or(false, |raw| {
        raw.starts_with("dev") || raw.starts_with("nightly")
    });
    let minor = otry!(otry!(minor).parse().ok());

    info!("detected rust 1.{minor}, nightly={nightly}");

    (minor, nightly)
}

/// Check that our list of cfg makes sense for the current target.
fn validate_cfg(list: &[Cfg], target_env: &str, target_os: &str, target_ptr_width: &str) {
    for cfg in list {
        match cfg {
            Cfg::LibcDenyWarnings => (),
            Cfg::LibcElfv2 => (),
            Cfg::LibcPauthtest => (),
            Cfg::EspidfPicolibc => (),
            Cfg::EspidfTime32 => (),

            Cfg::EmscriptenOldStatAbi => {
                assert_eq!(
                    target_os, "emscripten",
                    "{cfg:?} set on non-freebsd platform"
                );
            }

            Cfg::Freebsd10
            | Cfg::Freebsd11
            | Cfg::Freebsd12
            | Cfg::Freebsd13
            | Cfg::Freebsd14
            | Cfg::Freebsd15 => {
                assert_eq!(target_os, "freebsd", "{cfg:?} set on non-freebsd platform");
            }

            Cfg::VxworksLt25_09 => {
                assert_eq!(target_os, "vxworks", "{cfg:?} set on non-vxworks platform");
            }

            Cfg::GnuFileOffsetBits64 => {
                assert_eq!(target_env, "gnu", "{cfg:?} set on non-gnu platform");
                assert_eq!(target_ptr_width, "32", "{cfg:?} set on non-32-bit platform");
            }
            Cfg::GnuTimeBits64 => {
                assert_eq!(target_env, "gnu", "{cfg:?} set on non-gnu platform");
                assert_eq!(target_ptr_width, "32", "{cfg:?} set on non-32-bit platform");
                assert!(
                    list.contains(&Cfg::GnuFileOffsetBits64),
                    "{cfg:?} set without 64-bit off_t"
                )
            }
            Cfg::LinuxTimeBits64 => {
                assert_eq!(target_os, "linux", "{cfg:?} set on non-linux platform");
            }
            Cfg::MuslV1_2 => {
                assert!(
                    matches!(target_env, "musl" | "ohos"),
                    "{cfg:?} set with env {target_env}"
                );
            }
            Cfg::Musl32Time64 => {
                assert!(
                    matches!(target_env, "musl" | "ohos"),
                    "{cfg:?} set with env {target_env}"
                );
                assert_eq!(target_ptr_width, "32", "{cfg:?} set on non-32-bit platform");
                assert!(
                    list.contains(&Cfg::MuslV1_2),
                    "{cfg:?} set on non-musl1.2 platform"
                )
            }
            Cfg::MuslRedirTime64 => {
                assert!(
                    matches!(target_env, "musl" | "ohos"),
                    "{cfg:?} set with env {target_env}"
                );
                assert_eq!(target_ptr_width, "32", "{cfg:?} set on non-32-bit platform");
                assert!(
                    list.contains(&Cfg::MuslV1_2),
                    "{cfg:?} set on non-musl1.2 platform"
                )
            }
            Cfg::Uclibc32Time64 => {
                assert_eq!(target_env, "uclibc", "{cfg:?} set on non-gnu platform");
                assert_eq!(target_ptr_width, "32", "{cfg:?} set on non-32-bit platform");
                if target_os == "linux" {
                    assert!(
                        list.contains(&Cfg::LinuxTimeBits64),
                        "missing 64-bit time_t Linux config"
                    )
                }
            }
        }
    }
}

fn which_freebsd() -> Option<i32> {
    let output = Command::new("freebsd-version").output().ok()?;
    if !output.status.success() {
        return None;
    }

    let stdout = String::from_utf8(output.stdout).ok()?;

    match &stdout {
        s if s.starts_with("10") => Some(10),
        s if s.starts_with("11") => Some(11),
        s if s.starts_with("12") => Some(12),
        s if s.starts_with("13") => Some(13),
        s if s.starts_with("14") => Some(14),
        s if s.starts_with("15") => Some(15),
        // Until we have a need to treat them separately, treat FreeBSD 16 like 15
        s if s.starts_with("16") => Some(15),
        _ => None,
    }
}

fn emcc_version_code() -> Option<u64> {
    let emcc = if cfg!(target_os = "windows") {
        "emcc.bat"
    } else {
        "emcc"
    };

    let output = Command::new(emcc).arg("-dumpversion").output().ok()?;
    if !output.status.success() {
        return None;
    }

    let version = String::from_utf8(output.stdout).ok()?;

    // Some Emscripten versions come with `-git` attached, so split the
    // version string also on the `-` char.
    let mut pieces = version.trim().split(['.', '-']);

    let major = pieces.next().and_then(|x| x.parse().ok()).unwrap_or(0);
    let minor = pieces.next().and_then(|x| x.parse().ok()).unwrap_or(0);
    let patch = pieces.next().and_then(|x| x.parse().ok()).unwrap_or(0);

    Some(major * 10000 + minor * 100 + patch)
}

/// Retrieve the VxWorks release version from the environment variable set by the VxWorks build
/// environment, in `(minor, patch)` form. Currently the only major version supported by Rust
/// is 7.
fn vxworks_version_code() -> Option<(u32, u32)> {
    let version = env::var("WIND_RELEASE_ID").ok()?;

    let mut pieces = version.trim().split(['.']);

    let major: u32 = pieces.next().and_then(|x| x.parse().ok()).unwrap_or(0);
    let minor: u32 = pieces.next().and_then(|x| x.parse().ok()).unwrap_or(0);

    Some((major, minor))
}

fn set_cfg(cfg: Cfg) {
    println!("cargo:rustc-cfg={cfg}");
    info!("setting config `{cfg}`");
}

/// Return true if the env is set to a value other than `0`.
fn env_flag(key: &str) -> bool {
    match env::var(key) {
        Ok(x) if x == "0" => false,
        Err(VarError::NotPresent) => false,
        Err(VarError::NotUnicode(_)) => panic!("non-unicode var for `{key}`"),
        Ok(_) => true,
    }
}
