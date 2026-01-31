use better_any::nightly::downcast_any;
use std::any::Any;

fn testlt<'a, 'b>(any: &'a dyn Any) -> &'b i32 {
    downcast_any(any).unwrap()
}

fn main() {}
