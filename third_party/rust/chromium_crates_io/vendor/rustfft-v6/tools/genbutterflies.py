# A simple Python script to generate the code for odd-sized optimized DFTs
# The generated code is simply printed in the terminal.
# This is only intended for prime lengths, where the usual tricks can't be used.
# The generated code is O(n^2), but for short lengths this is still faster than fancier algorithms.
# Example, make a length 5 Dft:
# > python genbutterflies.py 5
# Output:
# let x14p = *buffer.get_unchecked(1) + *buffer.get_unchecked(4);
# let x14n = *buffer.get_unchecked(1) - *buffer.get_unchecked(4);
# let x23p = *buffer.get_unchecked(2) + *buffer.get_unchecked(3);
# let x23n = *buffer.get_unchecked(2) - *buffer.get_unchecked(3);
# let sum = *buffer.get_unchecked(0) + x14p + x23p;
# let b14re_a = buffer.get_unchecked(0).re + self.twiddle1.re*x14p.re + self.twiddle2.re*x23p.re;
# let b14re_b = self.twiddle1.im*x14n.im + self.twiddle2.im*x23n.im;
# let b23re_a = buffer.get_unchecked(0).re + self.twiddle2.re*x14p.re + self.twiddle1.re*x23p.re;
# let b23re_b = self.twiddle2.im*x14n.im + -self.twiddle1.im*x23n.im;
# 
# let b14im_a = buffer.get_unchecked(0).im + self.twiddle1.re*x14p.im + self.twiddle2.re*x23p.im;
# let b14im_b = self.twiddle1.im*x14n.re + self.twiddle2.im*x23n.re;
# let b23im_a = buffer.get_unchecked(0).im + self.twiddle2.re*x14p.im + self.twiddle1.re*x23p.im;
# let b23im_b = self.twiddle2.im*x14n.re + -self.twiddle1.im*x23n.re;
# 
# let out1re = b14re_a - b14re_b;
# let out1im = b14im_a + b14im_b;
# let out2re = b23re_a - b23re_b;
# let out2im = b23im_a + b23im_b;
# let out3re = b23re_a + b23re_b;
# let out3im = b23im_a - b23im_b;
# let out4re = b14re_a + b14re_b;
# let out4im = b14im_a - b14im_b;
# *buffer.get_unchecked_mut(0) = sum;
# *buffer.get_unchecked_mut(1) = Complex{ re: out1re, im: out1im };
# *buffer.get_unchecked_mut(2) = Complex{ re: out2re, im: out2im };
# *buffer.get_unchecked_mut(3) = Complex{ re: out3re, im: out3im };
# *buffer.get_unchecked_mut(4) = Complex{ re: out4re, im: out4im };
#
#
# This required the Butterfly5 to already exist, with twiddles defined like this:
# pub struct Butterfly5<T> {
#     twiddle1: Complex<T>,
#     twiddle2: Complex<T>,
# 	direction: FftDirection,
# }
# 
# With twiddle values:
# twiddle1: Complex<T> = twiddles::single_twiddle(1, 5, direction);
# twiddle2: Complex<T> = twiddles::single_twiddle(2, 5, direction);

import sys

len = int(sys.argv[1])

halflen = int((len+1)/2)

for n in range(1, halflen):
    print(f"let x{n}{len-n}p = buffer.load({n}) + buffer.load({len-n});")
    print(f"let x{n}{len-n}n = buffer.load({n}) - buffer.load({len-n});")

row = ["let sum = buffer.load(0)"]
for n in range(1, halflen):
    row.append(f"x{n}{len-n}p")
print(" + ".join(row) + ";")

for n in range(1, halflen):
    row = [f"let b{n}{len-n}re_a = buffer.load(0).re"]
    for m in range(1, halflen):
        mn = (m*n)%len
        if mn > len/2:
            mn = len-mn
        row.append(f"self.twiddle{mn}.re*x{m}{len-m}p.re")
    print(" + ".join(row) + ";")
    row = []
    for m in range(1, halflen):
        mn = (m*n)%len
        if mn > len/2:
            mn = len-mn
            row.append(f"-self.twiddle{mn}.im*x{m}{len-m}n.im")
        else:
            row.append(f"self.twiddle{mn}.im*x{m}{len-m}n.im")
    print(f"let b{n}{len-n}re_b = " + " + ".join(row) + ";")
print("")

for n in range(1, halflen):
    row = [f"let b{n}{len-n}im_a = buffer.load(0).im"]
    for m in range(1, halflen):
        mn = (m*n)%len
        if mn > len/2:
            mn = len-mn
        row.append(f"self.twiddle{mn}.re*x{m}{len-m}p.im")
    print(" + ".join(row) + ";")
    row = []
    for m in range(1, halflen):
        mn = (m*n)%len
        if mn > len/2:
            mn = len-mn
            row.append(f"-self.twiddle{mn}.im*x{m}{len-m}n.re")
        else:
            row.append(f"self.twiddle{mn}.im*x{m}{len-m}n.re")
    print(f"let b{n}{len-n}im_b = " + " + ".join(row) + ";")
print("")

for n in range(1,len):
    nfold = n
    sign_re = "-"
    sign_im = "+"
    if n > len/2:
        nfold = len-n
        sign_re = "+"
        sign_im = "-"
    print(f"let out{n}re = b{nfold}{len-nfold}re_a {sign_re} b{nfold}{len-nfold}re_b;")
    print(f"let out{n}im = b{nfold}{len-nfold}im_a {sign_im} b{nfold}{len-nfold}im_b;")

print("buffer.store(sum, 0);")
for n in range(1,len):
    print(f"buffer.store(Complex{{ re: out{n}re, im: out{n}im }}, {n})")