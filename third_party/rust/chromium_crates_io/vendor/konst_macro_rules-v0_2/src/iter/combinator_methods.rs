#[doc(hidden)]
#[macro_export]
macro_rules! __process_iter_args {
    (
        $callback_macro:tt
        $fixed_arguments:tt
        $other_args:tt
    ) => (
        $crate::__::compile_error!{"expected iterator argument"}
    );
    (
        $callback_macro:tt
        $fixed_arguments:tt
        $other_args:tt
        $iter:expr $(, $method:ident $(($($args:tt)*))? )* $(,)*
        $( => $($rem:tt)*)?
    ) => ({
        $(
            $crate::__cim_assert_has_args!{ $method $(($($args)*))? }
        )*

        $crate::iter::__cim_preprocess_methods !{
            (
                ((iter = $crate::into_iter_macro!($iter));)
                $callback_macro
                $fixed_arguments
                $other_args
                (
                    $($method($($($args)*)?),)*
                    $( => $($rem)* )?
                )
            )

            [next]

            $($method $method ($($($args)*)?),)*
        }
    });
    (
        $callback_macro:tt
        $fixed_arguments:tt
        $other_args:tt
        $iter:expr,
            $method0:ident $(($($args0:tt)*))?
            .$method1:ident
        $($rem:tt)*
    ) => ({
        $crate::__::compile_error!{"\
            iterator methods in this macro are comma-separated\
        "}
    })
}

#[doc(hidden)]
#[macro_export]
macro_rules! __call_iter_methods {
    (
        $fixed:tt
        ($vars:tt $macro:tt $prev_args:tt $label:tt next_back $allowed_methods:tt)
        $item:ident $iters:tt
        rev($($args:tt)*), $($rem:tt)*
    ) => ({
        $crate::__cim_error_on_args!{rev($($args)*)}

        $crate::__call_iter_methods!{
            ($vars $macro $prev_args $label next $allowed_methods)
            ($vars $macro $prev_args $label next $allowed_methods)
            $item
            $iters
            $($rem)*
        }
    });
    (
        $fixed:tt
        ($vars:tt $macro:tt $prev_args:tt $label:tt next $allowed_methods:tt)
        $item:ident $iters:tt
        rev($($args:tt)*), $($rem:tt)*
    ) => ({
        $crate::__cim_error_on_args!{rev($($args)*)}

        $crate::__call_iter_methods!{
            ($vars $macro $prev_args $label next_back $allowed_methods)
            ($vars $macro $prev_args $label next_back $allowed_methods)
            $item
            $iters
            $($rem)*
        }
    });
    (
        (
            $vars:tt
            $macro:tt
            $prev_args:tt
            $label:tt
            $next_fn:tt
            $allowed_methods:ident
        )
        (($iter_var:ident $($rem_vars:ident)*) $($rem_fixed:tt)*)
        $item:ident
        ($($iters:tt)*)
        zip($($args:tt)*), $($rem:tt)*
    ) => (
        $crate::__call_iter_methods!{
            (($($rem_vars)*) $($rem_fixed)*)
            (($($rem_vars)*) $($rem_fixed)*)
            $item
            ( $($iters)* (
                {}

                let $item = if let $crate::__::Some((elem_, next_)) = $iter_var.$next_fn() {
                    $iter_var = next_;

                    ($item, elem_)
                } else {
                    $crate::__cim_break!{(($($rem_vars)*) $($rem_fixed)*)}
                };
            ))

            $($rem)*
        }
    );
    (
        $fixed:tt
        (($iter_var:ident $($rem_vars:ident)*) $($rem_fixed:tt)*)
        $item:ident ($($iters:tt)*)
        enumerate($($args:tt)*), $($rem:tt)*
    ) => ({
        $crate::__cim_error_on_args!{enumerate($($args)*)}

        $crate::__call_iter_methods!{
            (($($rem_vars)*) $($rem_fixed)*)
            (($($rem_vars)*) $($rem_fixed)*)
            $item
            ( $($iters)* (
                {}
                let $item = ($iter_var, $item);
                $iter_var+=1;
            ))
            $($rem)*
        }
    });
    (
        $fixed:tt
        (($var:ident $($rem_vars:ident)*) $($rem_fixed:tt)*)
        $item:ident
        ($($iters:tt)*)
        //``__cim_preprocess_methods` ensures that only one argument is passed
        take($($args:tt)*), $($rem:tt)*
    ) => (
        $crate::__call_iter_methods!{
            (($($rem_vars)*) $($rem_fixed)*)
            (($($rem_vars)*) $($rem_fixed)*)
            $item
            ( $($iters)* (
                {}
                if $var == 0 {
                    $crate::__cim_break!{$fixed}
                } else {
                    $var -= 1;
                }
            ))
            $($rem)*
        }
    );
    (
        $fixed:tt $fixedb:tt $item:ident ($($iters:tt)*)
        take_while($($pred:tt)*), $($rem:tt)*
    ) => (
        $crate::__call_iter_methods!{
            $fixed
            $fixedb
            $item
            ( $($iters)* (
                {}
                let cond: $crate::__::bool = $crate::utils::__parse_closure_1!(
                    ($crate::__cim_filter) ($item,) (take_while),
                    $($pred)*
                );
                if !cond {
                    $crate::__cim_break!{$fixed}
                }
            ))
            $($rem)*
        }
    );
    (
        $fixed:tt
        (($var:ident $($rem_vars:ident)*) $($rem_fixed:tt)*)
        $item:ident ($($iters:tt)*)
        //``__cim_preprocess_methods` ensures that only one argument is passed
        skip($($args:tt)*), $($rem:tt)*
    ) => (
        $crate::__call_iter_methods!{
            (($($rem_vars)*) $($rem_fixed)*)
            (($($rem_vars)*) $($rem_fixed)*)
            $item
            ( $($iters)* (
                {}
                if $var != 0 {
                    $var -= 1;
                    continue;
                }
            ))
            $($rem)*
        }
    );
    (
        $fixed:tt
        (($still_skipping:ident $($rem_vars:ident)*) $($rem_fixed:tt)*)
        $item:ident ($($iters:tt)*)
        skip_while($($pred:tt)*), $($rem:tt)*
    ) => (
        $crate::__call_iter_methods!{
            (($($rem_vars)*) $($rem_fixed)*)
            (($($rem_vars)*) $($rem_fixed)*)
            $item
            ( $($iters)* (
                {}
                $still_skipping = $still_skipping && $crate::utils::__parse_closure_1!(
                    ($crate::__cim_filter) ($item,) (skip_while),
                    $($pred)*
                );

                if $still_skipping {
                    continue;
                }
            ))
            $($rem)*
        }
    );
    (
        $fixed:tt $fixedb:tt $item:ident ($($iters:tt)*)
        filter($($pred:tt)*), $($rem:tt)*
    ) => (
        $crate::__call_iter_methods!{
            $fixed
            $fixedb
            $item
            ( $($iters)* (
                {}
                let cond = $crate::utils::__parse_closure_1!(
                    ($crate::__cim_filter) ($item,) (filter),
                    $($pred)*
                );
                if !cond {
                    continue;
                }
            ))
            $($rem)*
        }
    );
    (
        $fixed:tt $fixedb:tt $item:ident ($($iters:tt)*)
        filter_map($($args:tt)*), $($rem:tt)*
    ) => (
        $crate::__call_iter_methods!{
            $fixed
            $fixedb
            $item
            ( $($iters)* (
                {}
                let val: $crate::__::Option<_> = $crate::utils::__parse_closure_1!(
                    ($crate::__cim_map) ($item,) (filter_map),
                    $($args)*
                );

                let $item = match val {
                    $crate::__::Some(x) => x,
                    $crate::__::None => continue,
                };
            ))
            $($rem)*
        }
    );
    (
        $fixed:tt $fixedb:tt $item:ident ($($iters:tt)*)
        map($($args:tt)*), $($rem:tt)*
    ) => (
        $crate::__call_iter_methods!{
            $fixed
            $fixedb
            $item
            ( $($iters)* (
                {}
                let $item = $crate::utils::__parse_closure_1!(
                    ($crate::__cim_map) ($item,) (map),
                    $($args)*
                );
            ))
            $($rem)*
        }
    );
    (
        $fixed:tt $fixedb:tt $item:ident ($($iters:tt)*)
        copied($($args:tt)*), $($rem:tt)*
    ) => ({
        $crate::__cim_error_on_args!{copied($($args)*)}

        $crate::__call_iter_methods!{
            $fixed
            $fixedb
            $item
            ( $($iters)* (
                {}
                let $item = *$item;
            ))
            $($rem)*
        }
    });
    (
        $fixed:tt $fixedb:tt $item:ident $iters:tt
        flat_map($($args:tt)*), $($rem:tt)*
    ) => {
        $crate::__cim_output_layer!{
            $fixed
            $item
            $iters
            {}
            {
                $crate::utils::__parse_closure_1!{
                    ($crate::__cim_flat_map) ($fixed $item ($($rem)*)) (flat_map),
                    $($args)*
                }
            }
            {}
        }
    };
    (
        $fixed:tt $fixedb:tt $item:ident $iters:tt
        flatten($($args:tt)*), $($rem:tt)*
    ) => ({
        $crate::__cim_error_on_args!{flatten($($args)*)}
        $crate::__cim_output_layer!{
            $fixed
            $item
            $iters
            {}
            {
                $crate::__cim_flat_map! {
                    $fixed
                    $item
                    ($($rem)*)
                    |elem| elem
                }
            }
            {}
        }
    });
    (
        $fixed:tt
        ($vars:tt $macro:tt $prev_args:tt $label:tt $next_fn:tt adapter)
        $item:tt $iters:tt
        $comb:ident ($($args:tt)*), $($rem:tt)*
    ) => {
        $crate::iter::__cim_method_not_found_err!{$comb $comb}
    };
    (
        $fixed:tt
        (
            $vars:tt
            ($($macro:tt)*)
            ($($prev_args:tt)*)
            $label:tt
            $next_fn:tt
            consumer
        )
        $item:ident
        $iters:tt
        $($rem:tt)*
    ) => {
        $($macro)* ! {
            $($prev_args)*
            ($label $item $iters)
            $vars
            $item
            $($rem)*
        }
    };
    (
        $fixed:tt
        ($vars:tt ($($macro:tt)*) ($($prev_args:tt)*) $label:tt $next_fn:tt $allowed_methods:ident)
        $item:ident
        $iters:tt
        $($rem:tt)*
    ) => {
        $crate::__cim_output_layer!{
            $fixed
            $item
            $iters
            {  }
            { $($macro)* ! {@each $($prev_args)* ($item $allowed_methods), $($rem)*} }
            { $($macro)* ! {@end $($prev_args)* ($item $allowed_methods), $($rem)*} }
        }
    };
    ($($tt:tt)*) => {
        $crate::__::compile_error!{$crate::__::concat!(
            "Unsupported arguments: ",
            $crate::__::stringify!($($tt)*),
        )}
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __cim_output_layer {
    (
        (
            $vars:tt
            $macro:tt
            $prev_args:tt
            ($break_label:lifetime $($label:lifetime)?)
            $next_fn:tt
            $allowed_methods:ident
        )
        $item:ident
        (
            $((
                { $($var:ident = $var_expr:expr),* $(,)? }
                $($code:tt)*
            ))*
        )
        {$($extra_init:tt)*}
        $each:tt
        $finish:tt
    ) => ({
        match ($(($($var_expr,)*),)*) {
            ($(($(mut $var,)*),)*) => {
                $($extra_init)*
                $($label:)? loop {
                    $($($code)*)*
                    $each
                }
                $finish
            },
        }
    });
}

#[doc(hidden)]
#[macro_export]
macro_rules! __cim_filter {
    ($item:ident, |$elem:pat| $v:expr) => {{
        let $elem = &$item;
        // avoiding lifetime extension
        let v: $crate::__::bool = $v;
        v
    }};
}

#[doc(hidden)]
#[macro_export]
macro_rules! __cim_map {
    ($item:ident, |$elem:pat| $v:expr) => {{
        let $elem = $item;
        // allowing for lifetime extension of temporaries
        $v
    }};
}

#[doc(hidden)]
#[macro_export]
macro_rules! __cim_break {
    ((
        $vars:tt
        $macro:tt
        $prev_args:tt
        ($break_label:tt $($label:tt)?)
        $next_fn:tt
        $allowed_methods:ident
    )) => {
        break $break_label;
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __cim_flat_map {
    (
        (
            $vars:tt
            $macro:tt
            $prev_args:tt
            ($break_label:tt $($label:tt)?)
            $next_fn:tt
            $allowed_methods:ident
        )
        $item:ident
        ($($rem:tt)*)
        |$elem:pat| $v:expr
    ) => ({
        let $elem = $item;
        $crate::__call_iter_methods!{
            ($vars $macro $prev_args ($break_label) $next_fn $allowed_methods)
            ($vars $macro $prev_args ($break_label) $next_fn $allowed_methods)
            $item
            (
                (
                    {iter = $crate::into_iter_macro!($v)}
                    let $item = if let $crate::__::Some((elem_, next_)) = iter.$next_fn() {
                        iter = next_;
                        elem_
                    } else {
                        break;
                    };
                )
            )
            $($rem)*
        }
    });

}

#[doc(hidden)]
#[macro_export]
macro_rules! __cim_error_on_args {
    ($func:ident()) => ();
    ($func:ident ($($args:tt)*)) => {
        $crate::__::compile_error!{$crate::__::concat!{
            "`",
            $crate::__::stringify!($func),
            "` does not take arguments, passed: ",
            $crate::__::stringify!($($args)*),
        }}
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __cim_assert_has_args {
    ($func:ident ($($args:tt)*)) => {};
    ($func:ident) => {
        $crate::__::compile_error! {$crate::__::concat!{
            "method call expected arguments: ",
            $crate::__::stringify!($func),
        }}
    };
}
