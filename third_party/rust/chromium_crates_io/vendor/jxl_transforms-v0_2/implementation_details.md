# Implementation details of 2d-IDCT and reinterpreting-DCT

First of all, note that we only need to generate Nx2N, NxN, 2NxN transforms for
most sizes. The one exception are 8x32/32x8 IDCTs and 1x4/4x1
reinterpreting-DCTs.

The 4x1/1x4 reinterpreting-DCTs are very small and don't need special
considerations.

Large transforms use the same implementation strategy, but avoid increasing code
size by using size-generic code.

## Code generation
Code is generated with python scripts. The following bash snippet
generates the relevant files:

```bash
for i in 2 4 8 16 32
do
    python3 gen_idct.py $i > src/idct$i.rs
done 
for i in 2 4 8 16 32
do
    python3 gen_reinterpreting_dct.py $i > src/reinterpreting_dct$i.rs
done 
python3 gen_idct2d.py > src/idct2d.rs
python3 gen_reinterpreting_dct2d.py > src/reinterpreting_dct2d.rs
cargo fmt
```

## SIMD type selection

The compiler generates suboptimal code when mixing different vector sizes. 

Thus, as a first step we "downgrade" to the largest vector size that divides
both sizes of the transform.

## DCT/IDCT Implementation
Both the DCT and the IDCT use the same recursive algorithm used in libjxl to
compute a vector worth of DCTs/IDCTs.

## 2d transforms
The code is written to minimize transposition cost while still ensuring we load
full vectors at a time. We don't use any additional memory to store transposes.

Let K be vector length (which divides both sides of the DCT as per above).

### N x 2N transforms and 8x32 IDCT
For those transforms, the final output should be the same shape as the input.
Thus, we logically need to transpose, DCT, transpose and DCT. However, we can
instead first do a set of row-DCTs on K rows, transposing every KxK
sub-matrix in place in advance, then do a column-DCT on the first K columns,
and finally transpose the KxK sub-matrices in the columns again.

### N x N transforms
Square transforms are easy: we can do column-DCTs, then swap KxK blocks between
lower and upper triangular part of the block-matrix, going K columns by K columns
and transposing during the swap, and do a column-DCT after each group of columns
is complete.

### 2N x N IDCTs and 32x8 IDCT
For these transforms, we have a special implementation of 1D-IDCT that does part
of the transpose.
In particular, we transpose NxN blocks as in the square case. We are then left
with doing the row-DCT and interleaving blocks so that they go from stacked
horizontally to stacked vertically. Since that can be done by just reshuffling
individual columns of vectors, we merge that operation with the DCT.

### 2N x N DCTs
This is basically the same as the IDCTs, but in reverse order. Thus, the
"special" DCT applies a different permutation.
