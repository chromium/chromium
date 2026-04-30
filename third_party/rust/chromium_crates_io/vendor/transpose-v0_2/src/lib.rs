//! Utility for transposing multi-dimensional data stored as a flat slice
//!
//! This library treats Rust slices as flattened row-major 2D arrays, and provides functions to transpose these 2D arrays, so that the row data becomes the column data, and vice versa.
//! ```
//! // Create a 2D array in row-major order: the rows of our 2D array are contiguous,
//! // and the columns are strided
//! let input_array = vec![ 1, 2, 3,
//! 						4, 5, 6];
//! 
//! // Treat our 6-element array as a 2D 3x2 array, and transpose it to a 2x3 array
//! let mut output_array = vec![0; 6];
//! transpose::transpose(&input_array, &mut output_array, 3, 2);
//!
//! // The rows have become the columns, and the columns have become the rows
//! let expected_array =  vec![ 1, 4,
//!								2, 5,
//!								3, 6];
//! assert_eq!(output_array, expected_array);
//!
//! // If we transpose our data again, we should get our original data back.
//! let mut final_array = vec![0; 6];
//! transpose::transpose(&output_array, &mut final_array, 2, 3);
//! assert_eq!(final_array, input_array);
//! ```
//!
//! This library supports both in-place and out-of-place transposes. The out-of-place
//! transpose is much, much faster than the in-place transpose -- the in-place transpose should
//! only be used in situations where the system doesn't have enough memory to do an out-of-place transpose.
//!
//! The out-of-place transpose uses one out of three different algorithms depending on the length of the input array.
//!
//! - Small: simple iteration over the array. 
//! - Medium: divide the array into tiles of fixed size, and process each tile separately. 
//! - Large: recursively split the array into smaller parts until each part is of a good size for the tiling algorithm, and then transpose each part.  

#![no_std]

extern crate num_integer;
extern crate strength_reduce;

mod in_place;
mod out_of_place;
pub use in_place::transpose_inplace;
pub use out_of_place::transpose;
