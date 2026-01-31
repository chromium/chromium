use std::borrow::Borrow;
use std::cell::Cell;
// use crate::utils::Cow2::{Borrowed2, Owned2};

pub fn escape_whitespaces(data: impl Borrow<str>, escape_spaces: bool) -> String {
    let data = data.borrow();
    let mut res = String::with_capacity(data.len());
    data.chars().for_each(|ch| match ch {
        ' ' if escape_spaces => res.push('\u{00B7}'),
        '\t' => res.push_str("\\t"),
        '\n' => res.push_str("\\n"),
        '\r' => res.push_str("\\r"),
        _ => res.push(ch),
    });
    res
}

pub fn cell_update<T: Copy, F>(cell: &Cell<T>, f: F) -> T
where
    F: FnOnce(T) -> T,
{
    let old = cell.get();
    let new = f(old);
    cell.set(new);
    new
}

// pub enum Cow2<'a,Ref,T:Borrow<Ref> = Ref>{
//     Borrowed2(&'a Ref),
//     Owned2(T)
// }
//
// impl<'a,Ref,T:Borrow<Ref> + > Cow2<'a,Ref,T>{
//     fn to_owned(&self) -> T
// }
//
// impl<Ref,T:Borrow<Ref>> Borrow<Ref> for Cow2<'_,Ref,T>{
//     fn borrow(&self) -> &Ref {
//         match self{
//             Cow2::Borrowed2(x) => x,
//             Cow2::Owned2(x) => x.borrow(),
//         }
//     }
// }
//
// impl<'a,Ref,T:Borrow<Ref>> From<&'a Ref> for Cow2<'a,Ref,T>{
//     fn from(f: &'a Ref) -> Self {
//         Borrowed2(f)
//     }
// }
//
// impl<'a,Ref,T:Borrow<Ref>> From<T> for Cow2<'a,Ref,T>{
//     fn from(f: T) -> Self {
//         Owned2(f)
//     }
// }
