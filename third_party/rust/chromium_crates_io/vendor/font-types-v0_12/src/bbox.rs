use core::ops::Mul;

/// Minimum and maximum extents of a rectangular region.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct BoundingBox<T> {
    /// Minimum extent in the x direction-- the left side of a region.
    pub x_min: T,
    /// Minimum extent in the y direction. In a Y-up coordinate system,
    /// which is used by fonts, this represents the bottom of a region.
    pub y_min: T,
    /// Maximum extent in the x direction-- the right side of a region.
    pub x_max: T,
    /// Maximum extend in the y direction. In a Y-up coordinate system,
    /// which is used by fonts, this represents the top of the
    /// region.
    pub y_max: T,
}

impl<T> BoundingBox<T>
where
    T: Mul<Output = T> + Copy,
{
    /// Return a `BoundingBox` scaled by a scale factor of the same type
    /// as the stored bounds.
    ///
    /// Note: a negative scale factor will flip the bounding box, so you may
    /// want to call `normalize` after scaling.
    pub fn scale(&self, factor: T) -> Self {
        Self {
            x_min: self.x_min * factor,
            y_min: self.y_min * factor,
            x_max: self.x_max * factor,
            y_max: self.y_max * factor,
        }
    }
}

impl<T> BoundingBox<T>
where
    T: PartialOrd + Copy,
{
    /// Return `true` if the bounding box is normalized, i.e. `x_min <= x_max`
    /// and `y_min <= y_max`.
    pub fn is_normalized(&self) -> bool {
        self.x_min <= self.x_max && self.y_min <= self.y_max
    }

    /// Return a `BoundingBox` with the min and max values normalized so that
    /// `x_min <= x_max` and `y_min <= y_max`.
    pub fn normalize(&self) -> Self {
        let (x_min, x_max) = if self.x_min <= self.x_max {
            (self.x_min, self.x_max)
        } else {
            (self.x_max, self.x_min)
        };
        let (y_min, y_max) = if self.y_min <= self.y_max {
            (self.y_min, self.y_max)
        } else {
            (self.y_max, self.y_min)
        };
        Self {
            x_min,
            y_min,
            x_max,
            y_max,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalize() {
        let bbox = BoundingBox {
            x_min: 10,
            y_min: 20,
            x_max: 5,
            y_max: 15,
        };
        assert!(!bbox.is_normalized());
        let normalized = bbox.normalize();
        assert!(normalized.is_normalized());
        assert_eq!(normalized.x_min, 5);
        assert_eq!(normalized.y_min, 15);
        assert_eq!(normalized.x_max, 10);
        assert_eq!(normalized.y_max, 20);
    }

    #[test]
    fn normalize_negative_scaled() {
        let bbox = BoundingBox {
            x_min: 5,
            y_min: 5,
            x_max: 10,
            y_max: 10,
        };
        assert_eq!(bbox.normalize(), bbox);
        let scaled = bbox.scale(-2);
        assert!(!scaled.is_normalized());
        let normalized = scaled.normalize();
        assert_eq!(normalized.x_min, -20);
        assert_eq!(normalized.y_min, -20);
        assert_eq!(normalized.x_max, -10);
        assert_eq!(normalized.y_max, -10);
    }
}
