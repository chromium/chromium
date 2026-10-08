/// Search range values used in various tables
#[derive(Clone, Copy, Debug)]
pub struct SearchRange {
    pub search_range: u16,
    pub entry_selector: u16,
    pub range_shift: u16,
}

impl SearchRange {
    //https://github.com/fonttools/fonttools/blob/729b3d2960ef/Lib/fontTools/ttLib/ttFont.py#L1147
    /// calculate searchRange, entrySelector, and rangeShift
    ///
    /// these values are used in various places, such as [the base table directory]
    /// and [cmap format 4].
    ///
    /// [the base table directory]: https://learn.microsoft.com/en-us/typography/opentype/spec/otff#table-directory
    /// [cmap format 4]: https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values
    pub fn compute(n_items: usize, item_size: usize) -> Self {
        let entry_selector = (n_items as f64).log2().floor() as usize;
        let search_range = (2.0_f64.powi(entry_selector as i32) * item_size as f64) as usize;
        // The result doesn't really make sense with 0 tables but ... let's at least not fail
        let range_shift = (n_items * item_size).saturating_sub(search_range);
        SearchRange {
            search_range: search_range.try_into().unwrap(),
            entry_selector: entry_selector.try_into().unwrap(),
            range_shift: range_shift.try_into().unwrap(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// based on example at
    /// <https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values>
    #[test]
    fn simple_search_range() {
        let SearchRange {
            search_range,
            entry_selector,
            range_shift,
        } = SearchRange::compute(39, 2);
        assert_eq!((search_range, entry_selector, range_shift), (64, 5, 14));
    }

    #[test]
    fn search_range_no_crashy() {
        let foo = SearchRange::compute(0, 0);
        assert_eq!(
            (foo.search_range, foo.entry_selector, foo.range_shift),
            (0, 0, 0)
        )
    }
}
