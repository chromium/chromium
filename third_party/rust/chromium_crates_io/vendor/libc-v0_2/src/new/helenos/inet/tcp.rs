//! HelenOS TCP API
//!
//! * Header file: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/inet/include/inet/tcp.h>

use crate::errno_t;
pub use crate::inet::endpoint::*;
use crate::prelude::*;

s_no_extra_traits! {
    pub struct tcp_cb_t {
        pub connected: Option<unsafe extern "C" fn(conn: *mut tcp_conn_t)>,
        pub conn_failed: Option<unsafe extern "C" fn(conn: *mut tcp_conn_t)>,
        pub conn_reset: Option<unsafe extern "C" fn(conn: *mut tcp_conn_t)>,
        pub data_avail: Option<unsafe extern "C" fn(conn: *mut tcp_conn_t)>,
        pub urg_data: Option<unsafe extern "C" fn(conn: *mut tcp_conn_t)>,
    }
    pub struct tcp_listen_cb_t {
        pub new_conn:
            Option<unsafe extern "C" fn(listener: *mut tcp_listener_t, conn: *mut tcp_conn_t)>,
    }
}

extern_ty! {
    pub type tcp_t;
    pub type tcp_conn_t;
    pub type tcp_listener_t;
}

extern "C" {
    pub fn tcp_create(tcp: *mut *mut tcp_t) -> errno_t;
    pub fn tcp_destroy(tcp: *mut tcp_t);

    pub fn tcp_conn_create(
        tcp: *mut tcp_t,
        epp: *mut inet_ep2_t,
        callbacks: *mut tcp_cb_t,
        arg: *mut c_void,
        conn: *mut *mut tcp_conn_t,
    ) -> errno_t;
    pub fn tcp_conn_destroy(conn: *mut tcp_conn_t);

    pub fn tcp_listener_create(
        tcp: *mut tcp_t,
        ep: *mut inet_ep_t,
        listen_callbacks: *mut tcp_listen_cb_t,
        listen_arg: *mut c_void,
        conn_callbacks: *mut tcp_cb_t,
        conn_arg: *mut c_void,
        listener: *mut *mut tcp_listener_t,
    ) -> errno_t;
    pub fn tcp_listener_destroy(listener: *mut tcp_listener_t);

    pub fn tcp_conn_userptr(conn: *mut tcp_conn_t) -> *mut c_void;
    pub fn tcp_listener_userptr(listener: *mut tcp_listener_t) -> *mut c_void;

    pub fn tcp_conn_wait_connected(conn: *mut tcp_conn_t) -> errno_t;
    pub fn tcp_conn_send(conn: *mut tcp_conn_t, buf: *const c_void, len: size_t) -> errno_t;
    pub fn tcp_conn_send_fin(conn: *mut tcp_conn_t) -> errno_t;
    pub fn tcp_conn_push(conn: *mut tcp_conn_t) -> errno_t;
    pub fn tcp_conn_reset(conn: *mut tcp_conn_t) -> errno_t;

    pub fn tcp_conn_recv(
        conn: *mut tcp_conn_t,
        buf: *mut c_void,
        bufsize: size_t,
        received_len: *mut size_t,
    ) -> errno_t;
    pub fn tcp_conn_recv_wait(
        conn: *mut tcp_conn_t,
        buf: *mut c_void,
        bufsize: size_t,
        received_len: *mut size_t,
    ) -> errno_t;
}
