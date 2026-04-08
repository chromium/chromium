#[doc(hidden)]
#[macro_export]
macro_rules! zip_counter_and_last {
    (
        $(:: $(@$dummy:tt@)?)? $($macro:ident)::* ! $prev_args:tt
        ($($iter:tt)*)
        ($($counter:tt)*)
    ) => {
        $crate::__zip_counter_and_last_inner!{
            (($(:: $($dummy)?)? $($macro)::*) $prev_args)
            []
            ($($iter)*)
            ($($counter)*)
        }
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules! __zip_counter_and_last_inner {
    (
        $macro:tt
        [$($prev:tt)*]
        ($elem:tt $($iter:tt)+)
        ($counter:tt  $($rem_counter:tt)+)
    ) => {
        $crate::__zip_counter_and_last_inner!{
            $macro
            [$($prev)* prefix($elem $counter)]
            ($($iter)*)
            ($($rem_counter)*)
        }
    };
    (
        ( ($($macro:tt)*) { $($prev_args:tt)* } )
        [$($prev:tt)*]
        ($elem:tt)
        ($counter:tt  $($rem_counter:tt)*)
    ) => {
        $($macro)* !{
            $($prev_args)*

            $($prev)*
            last($elem $counter)
        }
    };
    (
        ( ($($macro:tt)*) { $($prev_args:tt)* } )
        [$($prev:tt)*]
        ()
        ($($rem_counter:tt)*)
    ) => {
        $($macro)* !{
            $($prev_args)*
            $($prev)*
        }
    };
}
