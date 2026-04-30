
use strength_reduce::StrengthReducedUsize;
use num_integer;

fn multiplicative_inverse(a: usize, n: usize) -> usize {
    // we're going to use a modified version extended euclidean algorithm
    // we only need half the output

    let mut t = 0;
    let mut t_new = 1;

    let mut r = n;
    let mut r_new = a;

    while r_new > 0 {
        let quotient = r / r_new;

        r = r - quotient * r_new;
        core::mem::swap(&mut r, &mut r_new);

        // t might go negative here, so we have to do a checked subtract
        // if it underflows, wrap it around to the other end of the modulo
        // IE, 3 - 4 mod 5  =  -1 mod 5  =  4
        let t_subtract = quotient * t_new;
        t = if t_subtract < t {
            t - t_subtract
        } else {
            n - (t_subtract - t) % n
        };
     	core::mem::swap(&mut t, &mut t_new);
    }

    t
}

/// Transpose the input array in-place.
///
/// Given an input array of size input_width * input_height, representing flattened 2D data stored in row-major order,
/// transpose the rows and columns of that input array, in-place.
///
/// Despite being in-place, this algorithm requires max(width, height) in scratch space.
///
/// ```
/// // row-major order: the rows of our 2D array are contiguous,
/// // and the columns are strided
/// let original_array = vec![ 1, 2, 3,
/// 						   4, 5, 6];
/// let mut input_array = original_array.clone();
///
/// // Treat our 6-element array as a 2D 3x2 array, and transpose it to a 2x3 array
/// // transpose_inplace requires max(width, height) scratch space, which is in this case 3
/// let mut scratch = vec![0; 3];
/// transpose::transpose_inplace(&mut input_array, &mut scratch, 3, 2);
///
/// // The rows have become the columns, and the columns have become the rows
/// let expected_array =  vec![ 1, 4,
///								2, 5,
///								3, 6];
/// assert_eq!(input_array, expected_array);
///
/// // If we transpose it again, we should get our original data back.
/// transpose::transpose_inplace(&mut input_array, &mut scratch, 2, 3);
/// assert_eq!(original_array, input_array);
/// ```
///
/// # Panics
///
/// Panics if `input.len() != input_width * input_height` or if `scratch.len() != max(width, height)`
pub fn transpose_inplace<T: Copy>(buffer: &mut [T], scratch: &mut [T], width: usize, height: usize) {
	assert_eq!(width.checked_mul(height), Some(buffer.len()));
	assert_eq!(core::cmp::max(width, height), scratch.len());

	let gcd = StrengthReducedUsize::new(num_integer::gcd(width, height));
	let a = StrengthReducedUsize::new(height / gcd);
	let b = StrengthReducedUsize::new(width / gcd);
	let a_inverse = multiplicative_inverse(a.get(), b.get());
	let strength_reduced_height = StrengthReducedUsize::new(height);

	let index_fn = |x, y| x + y * width;

	if gcd.get() > 1 {
		for x in 0..width {
			let column_offset = (x / b) % strength_reduced_height;
			let wrapping_point = height - column_offset;

			// wrapped rotation -- do the "right half" of the array, then the "left half"
	        for y in 0..wrapping_point {
	            scratch[y] = buffer[index_fn(x, y + column_offset)];
	        }
	        for y in wrapping_point..height {
	            scratch[y] = buffer[index_fn(x, y + column_offset - height)];
	        }

	        // copy the data back into the column
	        for y in 0..height {
	            buffer[index_fn(x, y)] = scratch[y];
	        }
	    }
	}

	// Permute the rows
	{
		let row_scratch = &mut scratch[0..width];
		
		for (y, row) in buffer.chunks_exact_mut(width).enumerate() {
		    for x in 0..width {
		    	let helper_val = if y <= height + x%gcd - gcd.get() { x + y*(width-1) } else { x + y*(width-1) + height };
		    	let (helper_div, helper_mod) = StrengthReducedUsize::div_rem(helper_val, gcd);

		    	let gather_x = (a_inverse * helper_div)%b + b.get()*helper_mod;
		        row_scratch[x] = row[gather_x];
		    }

		    row.copy_from_slice(row_scratch);
		}
	}

	// Permute the columns
	for x in 0..width {
		let column_offset = x % strength_reduced_height;
		let wrapping_point = height - column_offset;

		// wrapped rotation -- do the "right half" of the array, then the "left half"
	    for y in 0..wrapping_point {
	        scratch[y] = buffer[index_fn(x, y + column_offset)];
	    }
	    for y in wrapping_point..height {
            scratch[y] = buffer[index_fn(x, y + column_offset - height)];
        }

        // Copy the data back to the buffer, but shuffle it as we do so
	    for y in 0..height {
	    	let shuffled_y = (y * width - (y / a)) % strength_reduced_height;
	        buffer[index_fn(x, y)] = scratch[shuffled_y];
	    }
	}
}
