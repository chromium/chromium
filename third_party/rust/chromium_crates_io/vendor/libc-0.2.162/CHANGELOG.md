# Changelog

## [Unreleased]

## [0.2.162](https://github.com/rust-lang/libc/compare/0.2.161...0.2.162) - 2024-11-07

### Added

- Android: fix the alignment of `uc_mcontext` on arm64 <https://github.com/rust-lang/libc/pull/3894>
- Apple: add `host_cpu_load_info` <https://github.com/rust-lang/libc/pull/3916>
- ESP-IDF: add a time flag <https://github.com/rust-lang/libc/pull/3993>
- FreeBSD: add the `CLOSE_RANGE_CLOEXEC` flag<https://github.com/rust-lang/libc/pull/3996>
- FreeBSD: fix test errors regarding `__gregset_t` <https://github.com/rust-lang/libc/pull/3995>
- FreeBSD: fix tests on x86 FreeBSD 15 <https://github.com/rust-lang/libc/pull/3948>
- FreeBSD: make `ucontext_t` and `mcontext_t` available on all architectures  <https://github.com/rust-lang/libc/pull/3848>
- Haiku: add `getentropy` <https://github.com/rust-lang/libc/pull/3991>
- Illumos: add `syncfs` <https://github.com/rust-lang/libc/pull/3990>
- Illumos: add some recently-added constants <https://github.com/rust-lang/libc/pull/3999>
- Linux: add `ioctl` flags <https://github.com/rust-lang/libc/pull/3960>
- Linux: add epoll busy polling parameters <https://github.com/rust-lang/libc/pull/3922>
- NuttX: add `pthread_[get/set]name_np` <https://github.com/rust-lang/libc/pull/4003>
- RTEMS: add `arc4random_buf` <https://github.com/rust-lang/libc/pull/3989>
- Trusty OS: add initial support <https://github.com/rust-lang/libc/pull/3942>
- WASIp2: expand socket support <https://github.com/rust-lang/libc/pull/3981>

### Fixed

- Emscripten: don't pass `-lc` <https://github.com/rust-lang/libc/pull/4002>
- Hurd: change `st_fsid` field to `st_dev` <https://github.com/rust-lang/libc/pull/3785>
- Hurd: fix the definition of `utsname` <https://github.com/rust-lang/libc/pull/3992>
- Illumos/Solaris: fix `FNM_CASEFOLD` definition <https://github.com/rust-lang/libc/pull/4004>
- Solaris: fix all tests <https://github.com/rust-lang/libc/pull/3864>

### Other

- CI: Add loongarch64 <https://github.com/rust-lang/libc/pull/4000>
- CI: Check that semver files are sorted <https://github.com/rust-lang/libc/pull/4018>
- CI: Re-enable the FreeBSD 15 job <https://github.com/rust-lang/libc/pull/3988>
- Clean up imports and `extern crate` usage <https://github.com/rust-lang/libc/pull/3897>
- Convert `mode_t` constants to octal <https://github.com/rust-lang/libc/pull/3634>
- Remove the `wasm32-wasi` target that has been deleted upstream <https://github.com/rust-lang/libc/pull/4013>

## [0.2.161](https://github.com/rust-lang/libc/compare/0.2.160...0.2.161) - 2024-10-17

### Fixed

- OpenBSD: fix `FNM_PATHNAME` and `FNM_NOESCAPE` values <https://github.com/rust-lang/libc/pull/3983>

## [0.2.160](https://github.com/rust-lang/libc/compare/0.2.159...0.2.160) - 2024-10-17

### Added

- Android: add `PR_GET_NAME` and `PR_SET_NAME` <https://github.com/rust-lang/libc/pull/3941>
- Apple: add `F_TRANSFEREXTENTS` <https://github.com/rust-lang/libc/pull/3925>
- Apple: add `mach_error_string` <https://github.com/rust-lang/libc/pull/3913>
- Apple: add additional `pthread` APIs <https://github.com/rust-lang/libc/pull/3846>
- Apple: add the `LOCAL_PEERTOKEN` socket option <https://github.com/rust-lang/libc/pull/3929>
- BSD: add `RTF_*`, `RTA_*`, `RTAX_*`, and `RTM_*` definitions <https://github.com/rust-lang/libc/pull/3714>
- Emscripten: add `AT_EACCESS` <https://github.com/rust-lang/libc/pull/3911>
- Emscripten: add `getgrgid`, `getgrnam`, `getgrnam_r` and `getgrgid_r` <https://github.com/rust-lang/libc/pull/3912>
- Emscripten: add `getpwnam_r` and `getpwuid_r` <https://github.com/rust-lang/libc/pull/3906>
- FreeBSD: add `POLLRDHUP` <https://github.com/rust-lang/libc/pull/3936>
- Haiku: add `arc4random` <https://github.com/rust-lang/libc/pull/3945>
- Illumos: add `ptsname_r` <https://github.com/rust-lang/libc/pull/3867>
- Linux: add `fanotify` interfaces <https://github.com/rust-lang/libc/pull/3695>
- Linux: add `tcp_info` <https://github.com/rust-lang/libc/pull/3480>
- Linux: add additional AF_PACKET options <https://github.com/rust-lang/libc/pull/3540>
- Linux: make Elf constants always available <https://github.com/rust-lang/libc/pull/3938>
- Musl x86: add `iopl` and `ioperm` <https://github.com/rust-lang/libc/pull/3720>
- Musl: add `posix_spawn` chdir functions <https://github.com/rust-lang/libc/pull/3949>
- Musl: add `utmpx.h` constants <https://github.com/rust-lang/libc/pull/3908>
- NetBSD: add `sysctlnametomib`, `CLOCK_THREAD_CPUTIME_ID` and `CLOCK_PROCESS_CPUTIME_ID` <https://github.com/rust-lang/libc/pull/3927>
- Nuttx: initial support <https://github.com/rust-lang/libc/pull/3909>
- RTEMS: add `getentropy` <https://github.com/rust-lang/libc/pull/3973>
- RTEMS: initial support <https://github.com/rust-lang/libc/pull/3866>
- Solarish: add `POLLRDHUP`, `POSIX_FADV_*`, `O_RSYNC`, and `posix_fallocate` <https://github.com/rust-lang/libc/pull/3936>
- Unix: add `fnmatch.h` <https://github.com/rust-lang/libc/pull/3937>
- VxWorks: add riscv64 support <https://github.com/rust-lang/libc/pull/3935>
- VxWorks: update constants related to the scheduler  <https://github.com/rust-lang/libc/pull/3963>

### Changed

- Redox: change `ino_t` to be `c_ulonglong` <https://github.com/rust-lang/libc/pull/3919>

### Fixed

- ESP-IDF: fix mismatched constants and structs <https://github.com/rust-lang/libc/pull/3920>
- FreeBSD: fix `struct stat` on FreeBSD 12+ <https://github.com/rust-lang/libc/pull/3946>

### Other

- CI: Fix CI for FreeBSD 15 <https://github.com/rust-lang/libc/pull/3950>
- Docs: link to `windows-sys` <https://github.com/rust-lang/libc/pull/3915>

## [0.2.159](https://github.com/rust-lang/libc/compare/0.2.158...0.2.159) - 2024-09-24

### Added

- Android: add more `AT_*` constants in <https://github.com/rust-lang/libc/pull/3779>
- Apple: add missing `NOTE_*` constants in <https://github.com/rust-lang/libc/pull/3883>
- Hermit: add missing error numbers in <https://github.com/rust-lang/libc/pull/3858>
- Hurd: add `__timeval` for 64-bit support in <https://github.com/rust-lang/libc/pull/3786>
- Linux: add `epoll_pwait2` in <https://github.com/rust-lang/libc/pull/3868>
- Linux: add `mq_notify` in <https://github.com/rust-lang/libc/pull/3849>
- Linux: add missing `NFT_CT_*` constants in <https://github.com/rust-lang/libc/pull/3844>
- Linux: add the `fchmodat2` syscall in <https://github.com/rust-lang/libc/pull/3588>
- Linux: add the `mseal` syscall in <https://github.com/rust-lang/libc/pull/3798>
- OpenBSD: add `sendmmsg` and `recvmmsg` in <https://github.com/rust-lang/libc/pull/3831>
- Unix: add `IN6ADDR_ANY_INIT` and `IN6ADDR_LOOPBACK_INIT` in <https://github.com/rust-lang/libc/pull/3693>
- VxWorks: add `S_ISVTX` in <https://github.com/rust-lang/libc/pull/3768>
- VxWorks: add `vxCpuLib` and `taskLib` functions <https://github.com/rust-lang/libc/pull/3861>
- WASIp2: add definitions for `std::net` support in <https://github.com/rust-lang/libc/pull/3892>

### Fixed

- Correctly handle version checks when `clippy-driver` is used <https://github.com/rust-lang/libc/pull/3893>

### Changed

- EspIdf: change signal constants to c_int in <https://github.com/rust-lang/libc/pull/3895>
- HorizonOS: update network definitions in <https://github.com/rust-lang/libc/pull/3863>
- Linux: combine `ioctl` APIs in <https://github.com/rust-lang/libc/pull/3722>
- WASI: enable CI testing in <https://github.com/rust-lang/libc/pull/3869>
- WASIp2: enable CI testing in <https://github.com/rust-lang/libc/pull/3870>

## [0.2.158](https://github.com/rust-lang/libc/compare/0.2.157...0.2.158) - 2024-08-19

### Other
- WASI: fix missing `Iterator` with `rustc-dep-of-std` in <https://github.com/rust-lang/libc/pull/3856#event-13924913068>

## [0.2.157](https://github.com/rust-lang/libc/compare/0.2.156...0.2.157) - 2024-08-17

### Added

- Apple: add `_NSGetArgv`, `_NSGetArgc` and `_NSGetProgname` in <https://github.com/rust-lang/libc/pull/3702>
- Build: add `RUSTC_WRAPPER` support in <https://github.com/rust-lang/libc/pull/3845>
- FreeBSD: add `execvpe` support from 14.1 release in <https://github.com/rust-lang/libc/pull/3745>
- Fuchsia: add `SO_BINDTOIFINDEX`
- Linux: add `klogctl` in <https://github.com/rust-lang/libc/pull/3777>
- MacOS: add `fcntl` OFD commands in <https://github.com/rust-lang/libc/pull/3563>
- NetBSD: add `_lwp_park` in <https://github.com/rust-lang/libc/pull/3721>
- Solaris: add missing networking support in <https://github.com/rust-lang/libc/pull/3717>
- Unix: add `pthread_equal` in <https://github.com/rust-lang/libc/pull/3773>
- WASI: add `select`, `FD_SET`, `FD_ZERO`, `FD_ISSET ` in <https://github.com/rust-lang/libc/pull/3681>

### Fixed
- TEEOS: fix octal notation for `O_*` constants in <https://github.com/rust-lang/libc/pull/3841>

### Changed
- FreeBSD: always use freebsd12 when `rustc_dep_of_std` is set in <https://github.com/rust-lang/libc/pull/3723>

## [0.2.156](https://github.com/rust-lang/libc/compare/v0.2.155...v0.2.156) - 2024-08-15

### Added
- Apple: add `F_ALLOCATEPERSIST` in <https://github.com/rust-lang/libc/pull/3712>
- Apple: add `os_sync_wait_on_address` and related definitions in <https://github.com/rust-lang/libc/pull/3769>
- BSD: generalise `IPV6_DONTFRAG` to all BSD targets in <https://github.com/rust-lang/libc/pull/3716>
- FreeBSD/DragonFly: add `IP_RECVTTL`/`IPV6_RECVHOPLIMIT` in <https://github.com/rust-lang/libc/pull/3751>
- Hurd: add `XATTR_CREATE`, `XATTR_REPLACE` in <https://github.com/rust-lang/libc/pull/3739>
- Linux GNU: `confstr` API and `_CS_*` in <https://github.com/rust-lang/libc/pull/3771>
- Linux musl: add `preadv2` and `pwritev2` (1.2.5 min.) in <https://github.com/rust-lang/libc/pull/3762>
- VxWorks: add the constant `SOMAXCONN` in <https://github.com/rust-lang/libc/pull/3761>
- VxWorks: add a few errnoLib related constants in <https://github.com/rust-lang/libc/pull/3780>

### Fixed
- Solaris/illumos: Change `ifa_flags` type to u64 in <https://github.com/rust-lang/libc/pull/3729>
- QNX 7.0: Disable `libregex` in <https://github.com/rust-lang/libc/pull/3775>

### Changed
- QNX NTO: update platform support in <https://github.com/rust-lang/libc/pull/3815>
- `addr_of!(EXTERN_STATIC)` is now considered safe in <https://github.com/rust-lang/libc/pull/3776>

### Removed
- Apple: remove `rmx_state` in <https://github.com/rust-lang/libc/pull/3776>

### Other
- Update or remove CI tests that have been failing
