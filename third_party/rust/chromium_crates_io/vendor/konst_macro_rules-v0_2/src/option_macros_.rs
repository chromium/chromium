#[macro_export]
macro_rules! opt_unwrap {
    ($e:expr $(,)?) => {
        match $e {
            $crate::__::Some(x) => x,
            $crate::__::None => $crate::utils::panic("invoked `unwrap` macro on a `None` value"),
        }
    };
}

#[macro_export]
macro_rules! opt_unwrap_or {
    ($e:expr, $v:expr $(,)?) => {
        match ($e, $v) {
            ($crate::__::Some(x), _) => x,
            ($crate::__::None, value) => value,
        }
    };
}

#[macro_export]
macro_rules! opt_unwrap_or_else {
    ($e:expr, || $v:expr $(,)?) => {
        match $e {
            $crate::__::Some(x) => x,
            $crate::__::None => $v,
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take no arguments")
    };
    ($e:expr, $v:expr $(,)?) => {
        match $e {
            $crate::__::Some(x) => x,
            $crate::__::None => $v(),
        }
    };
}

#[macro_export]
macro_rules! opt_ok_or {
    ($e:expr, $v:expr $(,)?) => {
        match ($e, $v) {
            ($crate::__::Some(x), _) => $crate::__::Ok(x),
            ($crate::__::None, value) => $crate::__::Err(value),
        }
    };
}

#[macro_export]
macro_rules! opt_ok_or_else {
    ($e:expr, || $v:expr $(,)?) => {
        match $e {
            $crate::__::Some(x) => $crate::__::Ok(x),
            $crate::__::None => $crate::__::Err($v),
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take no arguments")
    };
    ($e:expr, $v:expr $(,)?) => {
        match $e {
            $crate::__::Some(x) => $crate::__::Ok(x),
            $crate::__::None => $crate::__::Err($v()),
        }
    };
}

#[macro_export]
macro_rules! opt_map {
    ($opt:expr, |$param:pat| $mapper:expr $(,)? ) => {
        match $opt {
            $crate::__::Some($param) => $crate::__::Some($mapper),
            $crate::__::None => $crate::__::None,
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($opt:expr, $function:path $(,)?) => {
        match $opt {
            $crate::__::Some(x) => $crate::__::Some($function(x)),
            $crate::__::None => $crate::__::None,
        }
    };
}

#[macro_export]
macro_rules! opt_and_then {
    ($opt:expr, |$param:pat| $mapper:expr $(,)? ) => {
        match $opt {
            $crate::__::Some($param) => $mapper,
            $crate::__::None => $crate::__::None,
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($opt:expr, $function:path $(,)?) => {
        match $opt {
            $crate::__::Some(x) => $function(x),
            $crate::__::None => $crate::__::None,
        }
    };
}

#[macro_export]
macro_rules! opt_flatten {
    ($opt:expr $(,)? ) => {
        match $opt {
            $crate::__::Some(x) => x,
            $crate::__::None => $crate::__::None,
        }
    };
}

#[macro_export]
macro_rules! opt_or_else {
    ($opt:expr, || $mapper:expr $(,)? ) => {
        match $opt {
            $crate::__::Some(x) => $crate::__::Some(x),
            $crate::__::None => $mapper,
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take no arguments")
    };
    ($opt:expr, $function:path $(,)?) => {
        match $opt {
            $crate::__::Some(x) => $crate::__::Some(x),
            $crate::__::None => $function(),
        }
    };
}

#[macro_export]
macro_rules! opt_filter {
    ($e:expr, |$param:pat| $v:expr $(,)?) => {
        match $e {
            $crate::__::Some(x)
                if {
                    let $param = &x;
                    $v
                } =>
            {
                $crate::__::Some(x)
            }
            _ => $crate::__::None,
        }
    };
    ($opt:expr, | $($anything:tt)* ) => {
        compile_error!("expected the closure to take a pattern as an argument")
    };
    ($e:expr, $function:path $(,)?) => {
        match $e {
            $crate::__::Some(x) if $function(&x) => $crate::__::Some(x),
            _ => $crate::__::None,
        }
    };
}
