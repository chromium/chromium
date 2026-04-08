use crate::{
    all_init_bytes::slice_as_bytes,
    TypeEq,
    TypeNe,
};


#[cfg(test)]
mod tests;


super::declare_const_param_type! {
    /// # Example
    /// 
    /// Using this marker type to implement dispatching of fields by name.
    /// 
    /// ```rust
    /// #![feature(adt_const_params)]
    /// #![feature(unsized_const_params)]
    /// 
    /// use typewit::{const_marker::Str, MakeTypeWitness};
    /// 
    /// let value = Stuff {
    ///     foo: 3,
    ///     bar: "hello",
    /// };
    /// 
    /// assert_eq!(value.get::<"foo">(), &3);
    /// assert_eq!(value.get::<"bar">(), &"hello");
    /// 
    /// pub struct Stuff<'a> {
    ///     foo: u32,
    ///     bar: &'a str,
    /// }
    /// 
    /// impl<'a> Stuff<'a> {
    ///     const fn get<const S: &'static str>(&self) -> &<Self as Field<S>>::Type 
    ///     where
    ///         FieldWit<Str<S>>: MakeTypeWitness,
    ///         Self: Field<S>,
    ///     {
    ///         let func = FnFieldTy::<Self>::NEW;
    /// 
    ///         match FieldWit::MAKE {
    ///             FieldWit::Foo(te) => te.map(func).in_ref().to_left(&self.foo),
    ///             FieldWit::Bar(te) => te.map(func).in_ref().to_left(&self.bar),
    ///         }
    ///     }
    /// }
    /// 
    /// typewit::type_fn! {
    ///     struct FnFieldTy<Struct>;
    ///  
    ///     impl<const S: &'static str> Str<S> => <Struct as Field<S>>::Type
    ///     where Struct: Field<S>
    /// }
    /// 
    /// trait Field<const S: &'static str> {
    ///     type Type: ?Sized;
    /// }
    /// impl<'a> Field<"foo"> for Stuff<'a> {
    ///     type Type = u32;
    /// }
    /// impl<'a> Field<"bar"> for Stuff<'a> {
    ///     type Type = &'a str;
    /// }
    /// 
    /// typewit::simple_type_witness! {
    ///     // the #[non_exhaustive] is necessary to be able to add fields to Stuff
    ///     #[non_exhaustive]
    ///     pub enum FieldWit {
    ///         Foo = Str<"foo">,
    ///         Bar = Str<"bar">,
    ///     }
    /// }
    /// 
    /// ```
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "adt_const_marker")))]
    Str(&'static str)

    fn equals(l, r) { u8_slice_eq(l.as_bytes(), r.as_bytes()) };
}


super::declare_const_param_type! {
    StrSlice(&'static [&'static str])

    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "adt_const_marker")))]
    fn equals(l, r) { str_slice_eq(l, r) };
}


super::declare_const_param_type! {
    BoolSlice(&'static [bool])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    CharSlice(&'static [char])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    U8Slice(&'static [u8])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    U16Slice(&'static [u16])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    U32Slice(&'static [u32])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    U64Slice(&'static [u64])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    U128Slice(&'static [u128])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    UsizeSlice(&'static [usize])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    I8Slice(&'static [i8])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    I16Slice(&'static [i16])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    I32Slice(&'static [i32])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    I64Slice(&'static [i64])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    I128Slice(&'static [i128])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}
super::declare_const_param_type! {
    IsizeSlice(&'static [isize])
    fn equals(l, r) { u8_slice_eq(slice_as_bytes(l), slice_as_bytes(r)) };
}

macro_rules! cmp_slice_of {
    ($left:ident, $right:ident, |$l:ident, $r:ident| $eq:expr) => {
        if $left.len() != $right.len() {
            false
        } else {
            let mut i = 0;
            loop {
                if i == $left.len() {
                    break true;
                } 

                let $l = &$left[i];
                let $r = &$right[i];
                if !$eq {
                    break false;
                }

                i += 1;
            }
        }
    }
}


const fn str_slice_eq(left: &[&str], right: &[&str]) -> bool {
    cmp_slice_of!{left, right, |l, r| u8_slice_eq(l.as_bytes(), r.as_bytes())}
}

const fn u8_slice_eq(left: &[u8], right: &[u8]) -> bool {
    cmp_slice_of!{left, right, |l, r| *l == *r}
}

