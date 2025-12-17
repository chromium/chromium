use better_any::Tid;
use better_any_derive::Tid;

#[derive(Tid)]
struct S3<'a, T>(&'a T);

#[derive(Tid)]
struct S5<'a, T: Trait>(&'a T);

#[derive(Tid)]
struct S6<'a, T: TraitLT<'a>>(&'a T);

#[derive(Tid)]
struct S7<'a, T: 'static>(&'a T);
