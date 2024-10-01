# Changelog

## [Unreleased]

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
