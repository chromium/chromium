use super::*;

#[doc(hidden)]
pub enum Param<T: Type<T>> {
    Owned(T),
    Borrowed(T::Abi),
}

impl<T: Type<T>> Param<T> {
    pub fn abi(&self) -> T::Abi {
        unsafe {
            match self {
                Self::Owned(item) => std::mem::transmute_copy(item),
                Self::Borrowed(borrowed) => std::mem::transmute_copy(borrowed),
            }
        }
    }
}

#[doc(hidden)]
pub trait TryIntoParam<T: Type<T>> {
    fn try_into_param(self) -> Result<Param<T>>;
}

impl<T> TryIntoParam<T> for Option<&T>
where
    T: ComInterface,
{
    fn try_into_param(self) -> Result<Param<T>> {
        match self {
            Some(from) => Ok(Param::Borrowed(from.abi())),
            None => Ok(Param::Borrowed(unsafe { std::mem::zeroed() })),
        }
    }
}

impl<T, U> TryIntoParam<T> for &U
where
    T: ComInterface,
    U: ComInterface,
    U: CanTryInto<T>,
{
    fn try_into_param(self) -> Result<Param<T>> {
        if U::CAN_INTO {
            Ok(Param::Borrowed(self.abi()))
        } else {
            Ok(Param::Owned(self.cast()?))
        }
    }
}

#[doc(hidden)]
pub trait CanTryInto<T>: ComInterface
where
    T: ComInterface,
{
    const CAN_INTO: bool = false;
}

impl<T, U> CanTryInto<T> for U
where
    T: ComInterface,
    U: ComInterface,
    U: CanInto<T>,
{
    const CAN_INTO: bool = true;
}

#[doc(hidden)]
pub trait CanInto<T>: Sized
where
    T: Clone,
{
    fn can_into(&self) -> &T {
        unsafe { std::mem::transmute(self) }
    }

    fn can_clone_into(&self) -> T {
        self.can_into().clone()
    }
}
impl<T> CanInto<T> for T where T: Clone {}

#[doc(hidden)]
pub trait IntoParam<T: TypeKind, C = <T as TypeKind>::TypeKind>: Sized
where
    T: Type<T>,
{
    fn into_param(self) -> Param<T>;
}

impl<T> IntoParam<T> for Option<&T>
where
    T: Type<T>,
{
    fn into_param(self) -> Param<T> {
        match self {
            Some(item) => Param::Borrowed(item.abi()),
            None => Param::Borrowed(unsafe { std::mem::zeroed() }),
        }
    }
}

impl<T, U> IntoParam<T, ReferenceType> for &U
where
    T: TypeKind<TypeKind = ReferenceType> + Clone,
    U: TypeKind<TypeKind = ReferenceType> + Clone,
    U: CanInto<T>,
{
    fn into_param(self) -> Param<T> {
        Param::Borrowed(self.abi())
    }
}

impl<T> IntoParam<T, ValueType> for &T
where
    T: TypeKind<TypeKind = ValueType> + Clone,
{
    fn into_param(self) -> Param<T> {
        Param::Borrowed(self.abi())
    }
}

impl<T, U> IntoParam<T, CopyType> for U
where
    T: TypeKind<TypeKind = CopyType> + Clone,
    U: TypeKind<TypeKind = CopyType> + Clone,
    U: CanInto<T>,
{
    fn into_param(self) -> Param<T> {
        unsafe { Param::Owned(std::mem::transmute_copy(&self)) }
    }
}
