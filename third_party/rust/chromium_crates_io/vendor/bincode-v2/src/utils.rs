pub trait Sealed {}

impl<T> Sealed for &mut T where T: Sealed {}
