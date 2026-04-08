// Generates a macro that takes a sequence of tokens with balanced `<` and `>` tokens
// and collects it until one of the additional rules decides
macro_rules! declare_generics_consuming_macro {(
    $_:tt $gen_consuming_macro_:ident = $gen_consuming_macro:ident
    $parsing_where:expr;


    $($additional_rules:tt)*
) => {

    #[doc(hidden)]
    #[macro_export]
    macro_rules! $gen_consuming_macro_ {
        (
            $fixed:tt
            [$_($counter:tt)*]
            [$_($prev:tt)*]
            [< $_($rem:tt)*]
        ) => {
            $crate::__::$gen_consuming_macro!{
                $fixed
                [1 $_($counter)*]
                [$_($prev)* <]
                [$_($rem)*]
            }
        };
        (
            $fixed:tt
            [$_($counter:tt)*]
            [$_($prev:tt)*]
            [<< $_($rem:tt)*]
        ) => {
            $crate::__::$gen_consuming_macro!{
                $fixed
                [1 1 $_($counter)*]
                [$_($prev)* <<]
                [$_($rem)*]
            }
        };
        (
            $fixed:tt
            [$counter0:tt $_($counter:tt)*]
            [$_($prev:tt)*]
            [> $_($rem:tt)*]
        ) => {
            $crate::__::$gen_consuming_macro!{
                $fixed
                [$_($counter)*]
                [$_($prev)* >]
                [$_($rem)*]
            }
        };
        (
            $fixed:tt
            [$counter0:tt $_($counter:tt)*]
            [$_($prev:tt)*]
            [>> $_($rem:tt)*]
        ) => {
            $crate::__::$gen_consuming_macro!{
                $fixed
                [$_($counter)*]
                [$_($prev)* >]
                [> $_($rem)*]
            }
        };
        (
            $fixed:tt
            [$counter0:tt $_($counter:tt)*]
            [$_($prev:tt)*]
            [>== $_($rem:tt)*]
        ) => {
            $crate::__::$gen_consuming_macro!{
                $fixed
                [$_($counter)*]
                [$_($prev)* >]
                [== $_($rem)*]
            }
        };
        (
            $fixed:tt
            [$counter0:tt $_($counter:tt)*]
            [$_($prev:tt)*]
            [>= $_($rem:tt)*]
        ) => {
            $crate::__::$gen_consuming_macro!{
                $fixed
                [$_($counter)*]
                [$_($prev)* >]
                [= $_($rem)*]
            }
        };

        $($additional_rules)*

        (
            $fixed:tt
            $counter:tt
            [$_($prev:tt)*]
            [$token:tt $_($rem:tt)*]
        ) => {
            $crate::__::$gen_consuming_macro!{
                $fixed
                $counter
                [$_($prev)* $token]
                [$_($rem)*]
            }
        };
        ( $fixed:tt $counter:tt [$_($prev:tt)*] [$_($token0:tt $_($other:tt)*)?]) => {
            $crate::__::compile_error!{$crate::__::concat!(
                "unexpected end of ", $parsing_where,": `",
                stringify!($_($prev)* $_($token0)?),
                "`",
            )}
        };
        ( $_($other:tt)* ) => {
            $crate::__::compile_error!{$crate::__::concat!(
                "bug: unhandled syntax in `typewit` macro: ",
                stringify!($_($other)*),
            )}
        };
    }

    pub use $gen_consuming_macro_ as $gen_consuming_macro;

}}

macro_rules! declare_parse_generics_macros {($_:tt [$($sep:tt)*] [$($err_token:tt)*]) => {
    #[doc(hidden)]
    #[macro_export]
    macro_rules! __parse_in_generics_ {
        $(
            ($fixed:tt $gen_args:tt $gen:tt [$_(,)? $err_token $_($rem:tt)*]) => {
                $crate::__::compile_error!{$crate::__::concat!(
                    "unexpected `",
                    $crate::__::stringify!($err_token),
                    "` in generic parameter list",
                )}
            };
        )*
        (
            ( $_($callback:ident)::* !($_($callback_args:tt)*) )
            // [$( (($($attrs)*) $generic_arg $phantom_arg = $default_val) )*]
            // Example of a single parameter list:
            // (() 'a (fn() -> &'a (),)) 
            // (() T (fn() -> $crate::__::PhantomData<T>,)) 
            // ((#[foo]) U (fn() -> $crate::__::PhantomData<U>,) = u32)
            // (() N ()) 
            // (() M () = 10)
            $gen_args:tt
            [$_(((/* no attributes */) $_($generics:tt)*))*]
            [$_(,)* $_(> $_($rem:tt)*)?]
        ) => {
            $_($callback)::* ! {
                $_($callback_args)*
                $gen_args
                [$_(($_($generics)*))*]
                []
                $_($_($rem)*)?
            }
        };
        // if there is at least one generic param with cfg attributes
        (
            ($_($fixed:tt)*)
            $gen_args:tt
            [$_((($_( $_(#[cfg($_($cfg:tt)+)])+ )?) $generics_first:tt $_($generics:tt)*))+]
            [$_(,)* $_(> $_($rem:tt)*)?]
        ) => {
            $crate::__::__pg_cfg_expansion!{
                ($_($fixed)* $_($_($rem)+)?)
                [] $gen_args
                [] [$_(($generics_first $_($generics)*))*]
                []
                [$_( ($_($generics_first all( $_( $_($cfg)+ ),* ))?) )+]
            }
        };
        (
            ($_($fixed:tt)*)
            $gen_args:tt
            [$_((($_(#[$_($attr:tt)*])*) $_($generics:tt)*))+]
            [$_(,)* $_(> $_($rem:tt)*)?]
        ) => {
            $_(
                $crate::__assert_valid_gen_attr!{ [] [$_(#[$_($attr)*])*] }
            )*
        };
        $(
            (
                $fixed:tt
                [$_($prev_gen_args:tt)*]
                [$_($prev_gen:tt)*]
                [
                    $_(,)?
                    $_(#[$_($attr:tt)*])*
                    $lt:lifetime $_(:
                        $_($lt_bound0:lifetime $_( + $lt_bound1:lifetime)*)?
                    )?
                    $_($sep $_($rem:tt)*)?
                ]
            ) => {
                $crate::__::__parse_in_generics!{
                    $fixed 
                    [$_($prev_gen_args)* ($lt (fn() -> &$lt (),) )]
                    [
                        $_($prev_gen)* 
                        (($_(#[$_($attr)*])*) $lt $_(: $_($lt_bound0 $_( + $lt_bound1 )* )?)?)
                    ]
                    [$_($sep  $_($rem)*)?]
                }
            };
            (
                $fixed:tt
                [$_($prev_gen_args:tt)*]
                [$_($prev_gen:tt)*]
                [
                    $_(,)? 
                    $_(#[$_($attr:tt)*])*
                    const $const:ident: $const_ty:ty $_(= $default:tt)?
                    $_($sep $_($rem:tt)*)?
                ]
            ) => {
                $crate::__::__parse_in_generics!{
                    $fixed 
                    [$_($prev_gen_args)* ($const () $_(= $default)?)]
                    [$_($prev_gen)* (($_(#[$_($attr)*])*) const $const: $const_ty)]
                    [$_($sep $_($rem)*)?]
                }
            };
        )*
        (
            $fixed:tt
            $prev_gen_args:tt
            $prev_gen:tt
            [
                $_(,)? $_(#[$_($attr:tt)*])* $ty:ident: $_($rem:tt)*
            ]
        ) => {
            $crate::__::__parse_ty_bounds!{
                (
                    $fixed 
                    $prev_gen_args
                    $prev_gen
                    ($_(#[$_($attr)*])*)
                    $ty
                )
                [] // counter for depth between < > pairs
                []
                [$_($rem)*]
            }
        };
        (
            $fixed:tt
            $prev_gen_args:tt
            $prev_gen:tt
            [
                $_(,)? $_(#[$_($attr:tt)*])* $ty:ident $_($rem:tt)*
            ]
        ) => {
            $crate::__::__pg_parsed_ty_bounds!{
                $fixed 
                $prev_gen_args
                $prev_gen
                ($_(#[$_($attr)*])*)
                $ty
                []
                $_($rem)*
            }
        };
        ($fixed:tt $gen_args:tt $gen:tt []) => {
            $crate::__::compile_error!{"unexpected end of generic parameter list"}
        };
        ($fixed:tt $gen_args:tt $gen:tt [$token0:tt $_($token1:tt $_($other:tt)*)?]) => {
            $crate::__::compile_error!{$crate::__::concat!(
                "unexpected token(s) in generic parameter list: `",
                stringify!($token0 $_($token1)?)
                "`"
            )}
        };
    }

    #[doc(hidden)]
    #[macro_export]
    macro_rules! __pg_parsed_ty_bounds_ {
        $(
            (
                $fixed:tt $gen_args:tt $gen:tt $attrs:tt $ty:tt $bound:tt
                [$err_token $_($rem:tt)*]
            ) => {
                $crate::__::compile_error!{$crate::__::concat!(
                    "unexpected `",
                    $crate::__::stringify!($err_token),
                    "` in type parameter declaration",
                )}
            };
        )*

        $(
            (
                $fixed:tt
                [$_($prev_gen_args:tt)*]
                [$_($prev_gen:tt)*]
                $attrs:tt
                $ty:ident 
                [$_($_($bound:tt)+)?]
                $_(= $default:ty)? $sep $_($rem:tt)*
            ) => {
                $crate::__::__parse_in_generics!{
                    $fixed 
                    [
                        $_($prev_gen_args)* 
                        ($ty (fn() -> $crate::__::PhantomData<$ty>,) $_(= $default)?)
                    ]
                    [$_($prev_gen)* ($attrs $ty $_(: $_($bound)+)?)]
                    [$sep $_($rem)*]
                }
            };
        )*
    }

    declare_generics_consuming_macro! {
        $ __parse_ty_bounds_ = __parse_ty_bounds
        "bound";

        ( ($_($fixed:tt)*) [] $prev:tt [$_(= $_($rem:tt)*)?] ) => {
            $crate::__::__pg_parsed_ty_bounds!{ $_($fixed)* $prev $_(= $_($rem)*)? }
        };

        $(
            ( ($_($fixed:tt)*) [] $prev:tt [$sep $_($rem:tt)*] ) => {
                $crate::__::__pg_parsed_ty_bounds!{ $_($fixed)* $prev $sep $_($rem)* }
            };
        )*

        $(
            ($fixed:tt $count:tt [$_($prev:tt)*] [$err_token $_($rem:tt)*]) => {
                $crate::__::compile_error!{$crate::__::concat!(
                    "unexpected end of bound: `",
                    stringify!($_($prev)* $err_token),
                    "`",
                )}
            };
        )*
    }
}} 

declare_parse_generics_macros!{$ [, >] [; where impl]}


pub use {
    __parse_in_generics_ as __parse_in_generics,
    __pg_parsed_ty_bounds_ as __pg_parsed_ty_bounds,
};

macro_rules! declare_pg_cfg_expansion {
($_:tt 
    $(
        [($deleted_lt_ty_marker_:ident) ($($gp_rule:tt)*) => {$($erase_marker_token:tt)*}]
    )*
) => {

    #[doc(hidden)]
    #[macro_export]
    macro_rules! __pg_cfg_expansion_ {

        (
            $fixed:tt
            [$_($prev_gen_args:tt)*] [$gen_arg:tt $_($rem_gen_args:tt)*]
            [$_($prev_generics:tt)*] [$generic:tt $_($rem_generics:tt)*]
            $deleted_lt_ty_marker:tt
            [() $_($rem_cfg:tt)*]
        ) => {
            $crate::__::__pg_cfg_expansion!{
                $fixed
                [$_($prev_gen_args)* $gen_arg] [ $_($rem_gen_args)*]
                [$_($prev_generics)* $generic] [ $_($rem_generics)*]
                $deleted_lt_ty_marker
                [$_($rem_cfg)*]
            }
        };
        $(
            (
                $fixed:tt
                [$_($prev_gen_args:tt)*] [$gen_arg:tt $_($rem_gen_args:tt)*]
                [$_($prev_generics:tt)*] [$generic:tt $_($rem_generics:tt)*]
                $_$deleted_lt_ty_marker_:tt
                [($($gp_rule)* $_($cfg:tt)+) $_($rem_cfg:tt)*]
            ) => {
                #[cfg($_($cfg)+)]
                $crate::__::__pg_cfg_expansion!{
                    $fixed
                    [$_($prev_gen_args)* $gen_arg] [ $_($rem_gen_args)*]
                    [$_($prev_generics)* $generic] [ $_($rem_generics)*]
                    $_$deleted_lt_ty_marker_
                    [$_($rem_cfg)*]
                }

                #[cfg(not($_($cfg)+))]
                $crate::__::__pg_cfg_expansion!{
                    $fixed
                    [$_($prev_gen_args)*] [ $_($rem_gen_args)*]
                    [$_($prev_generics)*] [ $_($rem_generics)*]
                    $($erase_marker_token)*
                    [$_($rem_cfg)*]
                }
            };
        )*

        (
            ( $_($callback:ident)::* !($_($callback_args:tt)*) $_($rem:tt)*)
            $gen_args:tt []
            $generics:tt []
            $deleted_lt_ty_marker:tt
            [] // no cfgs left
        ) => {
            $_($callback)::* ! {
                $_($callback_args)*
                $gen_args
                $generics
                $deleted_lt_ty_marker
                $_($rem)*
            }
        };
    }

}}

declare_pg_cfg_expansion!{
    $
    [(deleted_lt_ty_marker) (const) => {$deleted_lt_ty_marker}]
    [(deleted_lt_ty_marker) ($__gp:tt) => {[()]}]
}

pub use __pg_cfg_expansion_ as __pg_cfg_expansion;



#[doc(hidden)]
#[macro_export]
macro_rules! __parse_generics {
    // angle bracket generics
    (
        $fixed:tt
        [< $($generics:tt)*]
    ) => {
        $crate::__::__parse_in_generics!{
            $fixed
            []
            []
            [$($generics)*]
        }
    };
    // square bracket generic params
    // note: this is accepted so that simple_type_witness 
    // can still parse square bracket generics.
    (
        $fixed:tt
        [[$($generics:tt)*] $($rem:tt)*]
    ) => {
        $crate::__::__parse_in_generics!{
            $fixed
            []
            []
            [$($generics)*> $($rem)*]
        }
    };
    // no generics case
    (
        (
            $($callback:ident)::* !($($callback_args:tt)*)
        )
        [$($rem:tt)*]
    ) => {
        $($callback)::* ! {
            $($callback_args)*
            []
            []
            []
            $($rem)*
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __trailing_comma_for_where_clause {
    // fallback case
    (($($macro:ident)::* !($($args:tt)*)) [$($prev:tt)*] [] ) => {
        $($macro)::* !{$($args)* [$($prev)*] }
    };
    (($($macro:ident)::* !($($args:tt)*)) [$($($prev:tt)+)?] [$(,)? ; $($rem:tt)*] ) => {
        $($macro)::* !{$($args)* [$($($prev)+,)?] $($rem)* }
    };
    ($fixed:tt [$($prev:tt)*] [$t0:tt $($rem:tt)*]) => {
        $crate::__trailing_comma_for_where_clause!{
            $fixed [$($prev)* $t0] [$($rem)*]
        }
    };
}


// parses a where clause for an item where the where clause ends at any of:
// - `=`
// - `{...}`
// 
// The parsed tokens start with `where`, so that these can be parsed: 
// - there being no where clause
// - having a normal where clause
// - having a where clause delimited with brackets (e.g: `where[T: u32]`)
#[doc(hidden)]
#[macro_export]
macro_rules! __parse_where_clause_for_item {
    ($fixed:tt where [$($in_brackets:tt)*]: $($rem:tt)*) => {
        $crate::__::__parse_where_clause_for_item_inner!{
            $fixed [] [] [[$($in_brackets)*]: $($rem)*]
        }
    };
    // parses the `where [$where_predicates]` syntax that 
    // the simple_type_withness macro started with.
    ($fixed:tt where [$($in_brackets:tt)*] $($rem:tt)*) => {
        $crate::__trailing_comma_for_where_clause!{
            $fixed [] [$($in_brackets)*; $($rem)*]
        }
    };
    ($fixed:tt where $($rem:tt)*) => {
        $crate::__::__parse_where_clause_for_item_inner!{
            $fixed [] [] [$($rem)*]
        }
    };
    // no where clause
    (($($callback:ident)::* !($($callback_args:tt)*) ) $($rem:tt)*) => {
        $($callback)::* !{$($callback_args)* [] $($rem)*}
    };
}



declare_generics_consuming_macro! {
    $ __parse_where_clause_for_item_inner_ = __parse_where_clause_for_item_inner
    "where clause";

    // forward compatibility with `const { ... }` bounds,
    // dunno how likely const bounds are to be, but why not.
    ( $fixed:tt [] [$($prev:tt)*] [const {$($braced:tt)*} $($rem:tt)*] ) => {
        $crate::__::__parse_where_clause_for_item!{
            $fixed
            []
            [$($prev)* const {$($braced)*}]
            [$($rem)*]
        }
    };
    ( 
        ($($callback:ident)::* !($($callback_args:tt)*) )
        []
        [$($($prev:tt)+)?]
        [$(,)? = $($rem:tt)*] 
    ) => {
        $($callback)::* !{$($callback_args)* [$($($prev)+,)?] = $($rem)*}
    };
    (
        ($($callback:ident)::* !($($callback_args:tt)*) )
        []
        [$($($prev:tt)+)?]
        [$(,)? {$($braced:tt)*} $($rem:tt)*]
    ) => {
        $($callback)::* !{$($callback_args)* [$($($prev)+,)?] {$($braced)*} $($rem)*}
    };
    ($fixed:tt [] $prev:tt []) => {
        $crate::__::compile_error!{"unexpected end of where clause, expected rest of item"}
    };
}


declare_generics_consuming_macro! {
    $ __parse_generic_args_with_defaults_ = __parse_generic_args_with_defaults
    "generic arguments";

    (
        ($fixed:tt [$($prev_args:tt)*] [$($curr_gen_param:tt $($gen_params_rem:tt)*)?])
        []
        [$($prev_tokens:tt)*]
        [, $($rem:tt)*]
    ) => {
        $crate::__::__parse_generic_args_with_defaults! {
            ($fixed [$($prev_args)* $($prev_tokens)*,] [$($($gen_params_rem)*)?])
            []
            []
            [$($rem)*]
        }
    };

    (
        ($fixed:tt [$($prev_args:tt)*] [$($curr_gen_param:tt $($gen_params_rem:tt)*)?])
        []
        [$($prev_tokens:tt)+]
        [> $($rem:tt)*]
    ) => {
        $crate::__parse_generic_args_with_defaults__finish !{
            ($fixed [$($prev_args)* $($prev_tokens)*,] [$($($gen_params_rem)*)?])
            $($rem)*
        }
    };

    ($fixed:tt [] [] [> $($rem:tt)*]) => {
        $crate::__parse_generic_args_with_defaults__finish !{
            $fixed $($rem)*
        }
    };

    ($fixed:tt [] $prev:tt []) => {
        $crate::__::compile_error!{"unexpected end of generic arguments"}
    };
}




#[doc(hidden)]
#[macro_export]
macro_rules! __parse_generic_args_with_defaults__finish {
    (
        (
            (
                ($($callback:ident)::* !($($callback_args:tt)*) )
                $context:expr
            )

            [$($gen_args:tt)*]

            [
                $((
                    // gen_eff_def is either:
                    // - the default (if the generic parameter has one)
                    // - the name of the generic parameter (if it has no default)
                    (($($gen_eff_def:tt)*) $($__0:tt)*) 

                    (
                        // defined if the generic parameter does not have a default 
                        $([$gen_param:tt])? 
                        // defined if the generic parameter has a default
                        $(($($__1:tt)*) [$__gen_param:tt])?
                    )
                ))*
            ]
        )
        $($rem:tt)*
    ) => {
        $crate::__parse_generic_args_with_defaults__assert_only_defaults! {
            $context,
            [$($($gen_param)?)*]
        }

        $($callback)::* !{
            $($callback_args)* 
            [$($gen_args)* $($($gen_eff_def)* ,)*] 
            $($rem)*
        }
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules! __parse_generic_args_with_defaults__assert_only_defaults {
    (
        $context:expr,
        [$($($gen_param:tt)+)?]
    ) => {
        $(
            $crate::__::compile_error!{$crate::__::concat!{
                "expected these generic argument(s) for ", $context, " to be passed: "
                $( , stringify!($gen_param), )", "+
            }}
        )?
    };
}



#[doc(hidden)]
#[macro_export]
macro_rules! __assert_valid_gen_attr {
    ( $prev:tt [#[cfg($($tt:tt)*)] $($rem:tt)*]) => {
        $crate::__assert_valid_gen_attr!{ $prev [$($rem)*] }
    };
    ( [$($prev:tt)*] [#[$($tt:tt)*] $($rem:tt)*]) => {
        $crate::__assert_valid_gen_attr!{
            [$($prev)* #[$($tt)*]]
            [$($rem)*]
        }
    };
    ( [$(#[$attr:meta])*] []) => {
        $crate::__::compile_error!{$crate::__::concat!{
            "unsupported attribute(s) on generic parameter(s): "
            $( ,"`#[", stringify!($attr), "]`", )", "*
            "\nonly the `#[cfg(...)]` attribute is supported"
        }}
    };
}
