#[macro_export]
macro_rules! unwrap_ctx {
    ($e:expr $(,)?) => {
        match $e {
            $crate::__::Ok(x) => x,
            $crate::__::Err(e) => e.panic(),
        }
    };
}

#[macro_export]
macro_rules! res_unwrap_or {
    ($res:expr, $v:expr $(,)?) => {
        match ($res, $v) {
            ($crate::__::Ok(x), _) => x,
            ($crate::__::Err(_), value) => value,
        }
    };
}

#[macro_export]
macro_rules! res_unwrap_or_else {
    ($res:expr, |$param:pat| $expr:expr $(,)?) => {
        match $res {
            $crate::__::Ok(x) => x,
            $crate::__::Err($param) => $expr,
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($res:expr, $function:expr $(,)?) => {
        match $res {
            $crate::__::Ok(x) => x,
            $crate::__::Err(x) => $function(x),
        }
    };
}

#[macro_export]
macro_rules! res_unwrap_err_or_else {
    ($res:expr, |$param:pat| $expr:expr $(,)?) => {
        match $res {
            $crate::__::Ok($param) => $expr,
            $crate::__::Err(x) => x,
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($res:expr, $function:expr $(,)?) => {
        match $res {
            $crate::__::Ok(x) => $function(x),
            $crate::__::Err(x) => x,
        }
    };
}

#[macro_export]
macro_rules! res_ok {
    ($res:expr $(,)?) => {
        match $res {
            $crate::__::Ok(x) => $crate::__::Some(x),
            $crate::__::Err(_) => $crate::__::None,
        }
    };
}

#[macro_export]
macro_rules! res_err {
    ($res:expr $(,)?) => {
        match $res {
            $crate::__::Ok(_) => $crate::__::None,
            $crate::__::Err(x) => $crate::__::Some(x),
        }
    };
}

#[macro_export]
macro_rules! res_and_then {
    ($res:expr, |$param:pat| $expr:expr $(,)?) => {
        match $res {
            $crate::__::Ok($param) => $expr,
            $crate::__::Err(x) => $crate::__::Err(x),
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($res:expr, $function:expr $(,)?) => {
        match $res {
            $crate::__::Ok(param) => $function(param),
            $crate::__::Err(x) => $crate::__::Err(x),
        }
    };
}

#[macro_export]
macro_rules! res_map {
    ($res:expr, |$param:pat| $expr:expr $(,)?) => {
        match $res {
            $crate::__::Ok($param) => $crate::__::Ok($expr),
            $crate::__::Err(x) => $crate::__::Err(x),
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($res:expr, $function:expr $(,)?) => {
        match $res {
            $crate::__::Ok(param) => $crate::__::Ok($function(param)),
            $crate::__::Err(x) => $crate::__::Err(x),
        }
    };
}

#[macro_export]
macro_rules! res_map_err {
    ($res:expr, |$param:pat| $expr:expr $(,)?) => {
        match $res {
            $crate::__::Ok(x) => $crate::__::Ok(x),
            $crate::__::Err($param) => $crate::__::Err($expr),
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($res:expr, $function:expr $(,)?) => {
        match $res {
            $crate::__::Ok(x) => $crate::__::Ok(x),
            $crate::__::Err(x) => $crate::__::Err($function(x)),
        }
    };
}

#[macro_export]
macro_rules! res_or_else {
    ($res:expr, |$param:pat| $expr:expr $(,)?) => {
        match $res {
            $crate::__::Ok(x) => $crate::__::Ok(x),
            $crate::__::Err($param) => $expr,
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($res:expr, $function:expr $(,)?) => {
        match $res {
            $crate::__::Ok(x) => $crate::__::Ok(x),
            $crate::__::Err(x) => $function(x),
        }
    };
}
