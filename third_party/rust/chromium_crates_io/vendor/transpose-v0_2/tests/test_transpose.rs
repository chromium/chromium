extern crate transpose;

fn gen_data(width: usize, height: usize) -> Vec<usize> {
    (0..width*height).collect()
}

const BLOCK_SIZE: usize = 16;

#[test]
fn test_out_of_place_transpose() {
    let sizes = [
    	0, 1, 2,
    	BLOCK_SIZE - 1, BLOCK_SIZE, BLOCK_SIZE + 1, 
    	BLOCK_SIZE * 4 - 1, BLOCK_SIZE * 5, BLOCK_SIZE * 4 + 1
    	];

    for &width in &sizes {
        for &height in &sizes {
            let input = gen_data(width, height);
            let mut output = vec![0; width * height];

            transpose::transpose(&input, &mut output, width, height);

            for x in 0..width {
                for y in 0..height {
                    assert_eq!(input[x + y * width], output[y + x * height], "x = {}, y = {}", x, y);
                }
            }
        }
    }
}

#[test]
fn test_transpose_inplace() {

    for width in 1..10 {
        for height in 1..10 {
            let input = gen_data(width, height);
            let mut output = input.clone();
            let mut scratch = vec![usize::default(); std::cmp::max(width, height)];

            transpose::transpose_inplace(&mut output, &mut scratch, width, height);

            for x in 0..width {
                for y in 0..height {
                    assert_eq!(input[x + y * width], output[y + x * height], "x = {}, y = {}", x, y);
                }
            }
        }
    }
}