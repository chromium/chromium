//! Contains a [`Transform`] object holding values of an affine transformation matrix.

use super::instance::ResolvedPaint;
#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::*;
use read_fonts::{types::Matrix, ReadError};

/// A transformation matrix.
pub type Transform = Matrix<f32>;

impl TryFrom<&ResolvedPaint<'_>> for Transform {
    type Error = ReadError;

    fn try_from(paint: &ResolvedPaint<'_>) -> Result<Self, Self::Error> {
        match paint {
            ResolvedPaint::Rotate {
                angle,
                around_center,
                ..
            } => {
                let sin_v = (angle * 180.0).to_radians().sin();
                let cos_v = (angle * 180.0).to_radians().cos();
                let mut out_transform = Transform {
                    xx: cos_v,
                    xy: -sin_v,
                    yx: sin_v,
                    yy: cos_v,
                    ..Default::default()
                };

                fn scalar_dot_product(a: f32, b: f32, c: f32, d: f32) -> f32 {
                    a * b + c * d
                }

                if let Some(center) = around_center {
                    out_transform.dx = scalar_dot_product(sin_v, center.y, 1.0 - cos_v, center.x);
                    out_transform.dy = scalar_dot_product(-sin_v, center.x, 1.0 - cos_v, center.y);
                }
                Ok(out_transform)
            }
            ResolvedPaint::Scale {
                scale_x,
                scale_y,
                around_center,
                paint: _,
            } => {
                let mut out_transform = Transform {
                    xx: *scale_x,
                    yy: *scale_y,
                    ..Transform::default()
                };

                if let Some(center) = around_center {
                    out_transform.dx = center.x - scale_x * center.x;
                    out_transform.dy = center.y - scale_y * center.y;
                }
                Ok(out_transform)
            }
            ResolvedPaint::Skew {
                x_skew_angle,
                y_skew_angle,
                around_center,
                paint: _,
            } => {
                let tan_x = (x_skew_angle * 180.0).to_radians().tan();
                let tan_y = (y_skew_angle * 180.0).to_radians().tan();
                let mut out_transform = Transform {
                    xy: -tan_x,
                    yx: tan_y,
                    ..Transform::default()
                };

                if let Some(center) = around_center {
                    out_transform.dx = tan_x * center.y;
                    out_transform.dy = -tan_y * center.x;
                }
                Ok(out_transform)
            }
            ResolvedPaint::Transform {
                xx,
                yx,
                xy,
                yy,
                dx,
                dy,
                paint: _,
            } => Ok(Transform {
                xx: *xx,
                yx: *yx,
                xy: *xy,
                yy: *yy,
                dx: *dx,
                dy: *dy,
            }),
            ResolvedPaint::Translate { dx, dy, .. } => Ok(Transform {
                dx: *dx,
                dy: *dy,
                ..Default::default()
            }),
            _ => Err(ReadError::MalformedData(
                "ResolvedPaint cannot be converted into a transform.",
            )),
        }
    }
}
