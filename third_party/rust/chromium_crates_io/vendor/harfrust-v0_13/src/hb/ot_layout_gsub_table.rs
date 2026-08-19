use super::buffer::hb_buffer_t;
use super::font_funcs::FontFuncsDispatch;
use super::hb_font_t;
use super::ot_layout::*;
use super::ot_shape_plan::hb_ot_shape_plan_t;

pub fn substitute(
    plan: &hb_ot_shape_plan_t,
    face: &hb_font_t,
    font_funcs: &mut FontFuncsDispatch,
    buffer: &mut hb_buffer_t,
) {
    apply_layout_table(plan, face, font_funcs, buffer, face.ot_tables.gsub.as_ref());
}
