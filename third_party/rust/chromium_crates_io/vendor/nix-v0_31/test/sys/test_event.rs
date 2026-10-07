use libc::intptr_t;
use nix::sys::event::{EvFlags, EventFilter, FilterFlag, KEvent};
#[cfg(apple_targets)]
use nix::sys::event::{KEvent64, Kevent64Flags, Kqueue};

#[test]
fn test_struct_kevent() {
    use std::mem;

    let udata: intptr_t = 12345;
    let data: intptr_t = 0x1337;

    let actual = KEvent::new(
        0xdead_beef,
        EventFilter::EVFILT_READ,
        EvFlags::EV_ONESHOT | EvFlags::EV_ADD,
        FilterFlag::NOTE_CHILD | FilterFlag::NOTE_EXIT,
        data,
        udata,
    );
    assert_eq!(0xdead_beef, actual.ident());
    assert_eq!(EventFilter::EVFILT_READ, actual.filter().unwrap());
    assert_eq!(libc::EV_ONESHOT | libc::EV_ADD, actual.flags().bits());
    assert_eq!(libc::NOTE_CHILD | libc::NOTE_EXIT, actual.fflags().bits());
    assert_eq!(data, actual.data());
    assert_eq!(udata, actual.udata());
    assert_eq!(mem::size_of::<libc::kevent>(), mem::size_of::<KEvent>());
}

#[cfg(apple_targets)]
#[test]
fn test_struct_kevent64() {
    use std::mem;

    let actual = KEvent64::new(
        0xdead_beef,
        EventFilter::EVFILT_READ,
        EvFlags::EV_ONESHOT | EvFlags::EV_ADD,
        FilterFlag::NOTE_CHILD | FilterFlag::NOTE_EXIT,
        0x1337,
        12345,
        [0xaa, 0xbb],
    );
    assert_eq!(0xdead_beef, actual.ident());
    assert_eq!(EventFilter::EVFILT_READ, actual.filter().unwrap());
    assert_eq!(libc::EV_ONESHOT | libc::EV_ADD, actual.flags().bits());
    assert_eq!(libc::NOTE_CHILD | libc::NOTE_EXIT, actual.fflags().bits());
    assert_eq!(0x1337, actual.data());
    assert_eq!(12345, actual.udata());
    assert_eq!([0xaa, 0xbb], actual.ext());
    assert_eq!(
        mem::size_of::<libc::kevent64_s>(),
        mem::size_of::<KEvent64>()
    );
}

#[test]
fn test_kevent_filter() {
    let udata: intptr_t = 12345;

    let actual = KEvent::new(
        0xdead_beef,
        EventFilter::EVFILT_READ,
        EvFlags::EV_ONESHOT | EvFlags::EV_ADD,
        FilterFlag::NOTE_CHILD | FilterFlag::NOTE_EXIT,
        0x1337,
        udata,
    );
    assert_eq!(EventFilter::EVFILT_READ, actual.filter().unwrap());
}

#[test]
#[cfg(apple_targets)]
fn kevent64_empty_lists_zero_timeout() {
    let kq = Kqueue::new().expect("failed to create kqueue");
    let changelist: [KEvent64; 0] = [];
    let mut eventlist: [KEvent64; 0] = [];
    let timeout = libc::timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };
    let result = kq.kevent64(
        &changelist,
        &mut eventlist,
        Kevent64Flags::empty(),
        Some(timeout),
    );
    assert_eq!(result.expect("kevent64 should succeed"), 0);
}
