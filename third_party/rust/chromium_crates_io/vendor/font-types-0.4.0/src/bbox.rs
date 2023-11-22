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
