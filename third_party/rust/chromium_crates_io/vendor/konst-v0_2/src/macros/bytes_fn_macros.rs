macro_rules! impl_bytes_function {
    (
        strip_prefix;
        left = $left:expr;
        right = $right:expr;
        on_error = $on_error:expr,
    ) => {
        if $left.len() < $right.len() {
            $on_error
        }

        loop {
            match ($left, $right) {
                ([lb, rem_slice @ ..], [rb, rem_matched @ ..]) => {
                    $left = rem_slice;
                    $right = rem_matched;

                    if *lb != *rb {
                        $on_error
                    }
                }
                (rem, _) => {
                    $left = rem;
                    break;
                }
            }
        }
    };
    (
        strip_suffix;
        left = $left:expr;
        right = $right:expr;
        on_error = $on_error:expr,
    ) => {
        if $left.len() < $right.len() {
            $on_error
        }

        loop {
            match ($left, $right) {
                ([rem_slice @ .., lb], [rem_matched @ .., rb]) => {
                    $left = rem_slice;
                    $right = rem_matched;

                    if *lb != *rb {
                        $on_error
                    }
                }
                (rem, _) => {
                    $left = rem;
                    break;
                }
            }
        }
    };
}
