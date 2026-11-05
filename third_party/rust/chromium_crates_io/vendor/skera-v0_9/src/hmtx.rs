//! impl subset() for hmtx and HMTX

use read_fonts::{
    tables::{
        hhea::{Hhea, HheaExtended},
        hmtx::{Hmtx, HmtxExtended},
    },
    TableProvider, TopLevelTable,
};

crate::metrics::subset_metrics_table!(Hmtx, Hhea, hhea, false);
crate::metrics::subset_metrics_table!(HmtxExtended, HheaExtended, hhea_extended, true);
