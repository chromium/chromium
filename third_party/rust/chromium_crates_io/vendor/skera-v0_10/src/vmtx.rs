//! impl subset() for vmtx and VMTX

use read_fonts::{
    tables::{
        vhea::{Vhea, VheaExtended},
        vmtx::{Vmtx, VmtxExtended},
    },
    TableProvider, TopLevelTable,
};

crate::metrics::subset_metrics_table!(Vmtx, Vhea, vhea, false);
crate::metrics::subset_metrics_table!(VmtxExtended, VheaExtended, vhea_extended, true);
