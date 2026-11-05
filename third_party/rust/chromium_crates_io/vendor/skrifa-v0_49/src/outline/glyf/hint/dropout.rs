//! Dropout control requested by a glyph program.

use read_fonts::model::glyph::outline::DropoutControl;

/// Resolves the dropout control mode from the `scan_control` and `scan_type`
/// graphics state left by a glyph program.
pub(crate) fn from_scan_state(scan_control: bool, scan_type: i32) -> DropoutControl {
    if !scan_control {
        return DropoutControl::Ignore;
    }
    match scan_type {
        0 => DropoutControl::SimpleWithStubs,
        1 => DropoutControl::Simple,
        4 => DropoutControl::SmartWithStubs,
        5 => DropoutControl::Smart,
        _ => DropoutControl::Ignore,
    }
}

#[cfg(test)]
mod tests {
    use super::{from_scan_state, DropoutControl};

    // The mapping FreeType applies in ttgload.c when it converts the graphics
    // state into outline flags.
    #[test]
    fn scan_state_mapping() {
        for (scan_type, expected) in [
            (0, DropoutControl::SimpleWithStubs),
            (1, DropoutControl::Simple),
            (4, DropoutControl::SmartWithStubs),
            (5, DropoutControl::Smart),
            (2, DropoutControl::Ignore),
            (7, DropoutControl::Ignore),
        ] {
            assert_eq!(
                from_scan_state(true, scan_type),
                expected,
                "scan type {scan_type}"
            );
            // Nothing is restored unless the glyph asked for it.
            assert_eq!(from_scan_state(false, scan_type), DropoutControl::Ignore);
        }
    }
}
