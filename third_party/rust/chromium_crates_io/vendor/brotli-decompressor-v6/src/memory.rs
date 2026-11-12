#![allow(unused_macros)]
// use core::slice;

#[cfg(not(feature="unsafe"))]
macro_rules! fast_ref {
   (($slice : expr)[$index: expr]) => {
       &($slice)[$index]
   }; 
}

#[cfg(not(feature="unsafe"))]
macro_rules! fast_inner {
   (($slice : expr)[$index: expr]) => {
       ($slice)[$index]
   };
}

#[cfg(not(feature="unsafe"))]
macro_rules! fast_inner {
   (($slice : expr)[$index: expr]) => {
       ($slice)[$index]
   };
}

#[cfg(not(feature="unsafe"))]
macro_rules! fast {
   (($slice : expr)[$index: expr]) => {
       ($slice)[$index]
   };
   (($slice : expr)[$start: expr ; $end : expr]) => {
       &($slice)[$start .. $end]
   };
   (($slice : expr)[$start: expr ;]) => {
       &($slice)[$start .. ]
   };
   (($slice : expr)[; $end : expr]) => {
       &($slice)[.. $end ]
   };
}

macro_rules! fast_uninitialized {
    [$size : expr] => {[0; $size]};
    [$def_value : expr ; $size : expr] => {[$def_value; $size]};
}

#[cfg(not(feature="unsafe"))]
macro_rules! fast_mut {
   (($slice : expr)[$index: expr]) => {
       ($slice)[$index]
   };
   (($slice : expr)[$start: expr ; $end : expr]) => {
       &mut $slice[$start..$end]
   };
   (($slice : expr)[$start: expr ;]) => {
       &mut $slice[$start..]
   };
   (($slice : expr)[; $end : expr]) => {
       &mut $slice[..$end]
   };
}


#[cfg(feature="unsafe")]
#[allow(unused_unsafe)]
macro_rules! fast_ref {
   (($slice : expr)[$index: expr]) => {
       unsafe{$slice.get_unchecked($index)}
   };
}

#[cfg(feature="unsafe")]
macro_rules! fast_inner {
   (($slice : expr)[$index: expr]) => {
       *$slice.get_unchecked($index)
   };
}
// #[cfg(feature="unsafe")]
// macro_rules! fast_slice {
// (($slice : expr)[$index: expr]) => {
// unsafe{*$slice.slice().get_unchecked($index)}
// };
// }
//
#[cfg(feature="unsafe")]
macro_rules! fast {
   (($slice : expr)[$index: expr]) => {
       unsafe{*$slice.get_unchecked($index)}
   };
   (($slice : expr)[$start: expr ; $end : expr]) => {
       unsafe{::core::slice::from_raw_parts(($slice).as_ptr().offset($start as isize),
                                            $end - $start)}
   };
   (($slice : expr)[$start: expr ;]) => {
       unsafe{::core::slice::from_raw_parts(($slice).as_ptr().offset($start as isize),
                                                                     $slice.len() - $start)}
   };
   (($slice : expr)[; $end : expr]) => {
       unsafe{::core::slice::from_raw_parts(($slice).as_ptr(), $slice.len())}
   };
}

macro_rules! fast_slice {
   (($slice : expr)[$index: expr]) => {
       fast!(($slice.slice())[$index])
   };
   (($slice : expr)[$index: expr;]) => {
       fast!(($slice.slice())[$index;])
   };
   (($slice : expr)[$start :expr; $end: expr]) => {
       fast!(($slice.slice())[$start;$end])
   };
}
// macro_rules! fast_slice_ref {
// (($slice : expr)[$index: expr]) => {
// fast_ref!(($slice.slice())[$index])
// };
// (($slice : expr)[$index: expr]) => {
// fast_ref!(($slice.slice())[$index])
// };
// }
//
macro_rules! fast_slice_mut {
   (($slice : expr)[$index: expr]) => {
       fast_mut!(($slice.slice_mut())[$index])
   };
   (($slice : expr)[$index: expr;]) => {
       fast_mut!(($slice.slice_mut())[$index;])
   };
   (($slice : expr)[$start :expr;$end: expr]) => {
       fast_mut!(($slice.slice_mut())[$start;$end])
   };
}

#[cfg(feature="unsafe")]
macro_rules! fast_mut {
   (($slice : expr)[$index: expr]) => {
       *unsafe{$slice.get_unchecked_mut($index)}
   };
   (($slice : expr)[$start: expr ; $end : expr]) => {
       unsafe{($slice).get_unchecked_mut($start .. $end)}
   };
   (($slice : expr)[$start: expr ;]) => {
       unsafe{($slice).get_unchecked_mut($start ..)}
   };
   (($slice : expr)[; $end : expr]) => {
       unsafe{($slice).get_unchecked_mut(.. $end)}
   };
}

// pub fn indexk<T>(item : &[T], index : usize) -> &T {
//   return &item[index];
// return unsafe{item.get_unchecked(index)};
// }
//
// pub fn indexm<T>(item : &mut [T], index : usize) -> &mut T {
// return &mut item[index]
// return unsafe{item.get_unchecked_mut(index)};
// }
//
//
// pub fn slicek<T>(item : &[T], start : usize, end :usize) -> &[T] {
// return unsafe{slice::from_raw_parts(item.as_ptr().offset(start as isize), end - start)};
// }
//
// pub fn slicem<T>(item : &mut [T], start : usize, end :usize) -> &mut [T] {
// return unsafe{slice::from_raw_parts_mut(item.as_mut_ptr().offset(start as isize), end - start)};
// }
//
// pub fn slicemend<T>(item : &mut [T], start : usize) -> &mut [T] {
// return unsafe{slice::from_raw_parts_mut(item.as_mut_ptr().offset(start as isize),
//                                         item.len() - start)};
// }
//
