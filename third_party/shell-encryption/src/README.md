# Simple Homomorphic Encryption Library with Lattices (SHELL)

## Introduction

This project is a library for fully-homomorphic symmetric-key encryption. It
uses Ring Learning with Errors (RLWE)-based encryption to make it possible to
both add and multiply encrypted data. It uses modulus-switching to enable
arbitrary-depth homomorphic encryption (provided sufficiently large parameters
are set). RLWE is also believed to be secure in the face of quantum computers.

We intend this project to be both a useful experimental library and a
comprehensible tool for learning about and extending RLWE.

Discussion group: https://groups.google.com/forum/#!forum/shell-encryption/

**THIS IS NOT AN OFFICIAL GOOGLE PRODUCT**

## Fully Homomorphic Encryption

Fully homomorphic encryption is a form of encryption that makes it possible to
perform arbitrary computation on encrypted data.

For example, suppose that Alice creates a secret key *s* that she uses to
encrypt the numbers *2* and *3*, creating the ciphertexts *{2}<sub>s</sub>* and
*{3}<sub>s</sub>*. In an *additively homomorphic* encryption scheme, Bob (who
does not know the numbers inside the ciphertexts) could add the ciphertexts
together, generating the message *{5}<sub>s</sub>* (the encryption of the number
5), or multiply the ciphertexts together, generating the message
*{6}<sub>s</sub>* (the encryption of the number 6). The contents of the
resulting message remain hidden to Bob. When he gives this result back to Alice,
however, she can decrypt it to reveal the result of the computation.

When an encryption scheme is both additively and multiplicatively homomorphic,
it is said to be *fully homomorphic*.

Homomorphic encryption has a vast number of applications. As in the example
above, Alice can securely offload computation to another entity without worrying
that doing so will reveal any of her private information. Among many other
applications, it enables *private information retrieval* (PIR) - databases that
can serve user requests without learning which pieces of data the users
requested. (For more information on PIR, see [Communication--Computation Trade-offs in PIR](https://eprint.iacr.org/2019/1483.pdf).)

## Ring Learning with Errors

The homomorphic encryption scheme that this library implements is based off of
the Ring Learning with Errors problem. The security of this system derives from
the conjectured difficulty of finding the shortest vector in a lattice. This
document does not delve into the mathematical underpinnings of these proofs of
hardness.

The cryptosystem implemented in this library is from [Fully Homomorphic
Encryption from Ring-LWE and Security for Key Dependent
Messages](http://www.wisdom.weizmann.ac.il/~zvikab/localpapers/IdealHom.pdf).
The cryptosystem works as follows.

### Preliminaries

Each object in the cryptosystems, including keys, messages, and ciphertexts, are
made of polynomials. Each of these polynomials has degree *n-1*, where *n* is a
power of 2. In other words, each of these polynomials has *n* coefficients.

Each of the coefficients of these polynomials is an integer (mod *q*), where *q*
is a prime number equal to 1 (mod *2n*). When we operate on these polynomials,
we do so (mod *x<sup>n</sup>+1*).

We also need a modulus *t* that is much smaller than *q*. *log(t)* is the number
of bits of plaintext information we are able to fit into each coefficient of a
ciphertext polynomial. The importance of *t* will become apparent soon.

Finally, we need two other components: a binomial distribution *Y* with mean 0
and standard deviation *w*, where *w* is a parameter of the cryptosystem. The
importance of this distribution will become apparent soon.

### Key Generation

A secret key *s* is a polynomial whose *n* coefficients are drawn from the
distribution *Y*.

### Encryption

A message *m* that is to be encrypted must be a polynomial with *n*
coefficients, each of which is smaller than *t*.

To encrypt *m* with key *s*, select a polynomial *e* from the distribution *Y*
and a polynomial *a* from the uniform distribution (each coefficient of *a* is
chosen uniformly at random). *e* is a random amount of error that makes it
difficult to extract the error from the ciphertext. Both *a* and *e* are nonces
that should be regenerated freshly for each encryption.

The ciphertext consists of two polynomials. The first polynomial
*c<sub>0</sub> = as + m + et*. The second polynomial *c<sub>1</sub> = -a*. The
products between polynomials are computed using polynomial multiplications (mod
*x<sup>n</sup>+1*).

### Decryption

Given a ciphertext *(c<sub>0</sub>, c<sub>1</sub>)*, the ciphertext is decrypted
by computing *m* = *c<sub>0</sub> + sc<sub>1</sub>* (mod *t*) = *as + m + et -
as* (mod *t*) = *m + et* (mod *t*) = *m*. Recall that each coefficient of *m*
was already (mod *t*).

### Homomorphic Addition

To add two ciphertexts, simply add them component-wise. If we added the
ciphertexts *(c<sub>0x</sub>, c<sub>1x</sub>)* and *(c<sub>0y</sub>,
c<sub>1y</sub>)*, we would get the ciphertext *(c<sub>0x</sub> + c<sub>0y</sub>,
c<sub>1x</sub> + c<sub>1y</sub>)* = *(a<sub>x</sub>s + m<sub>x</sub> +
e<sub>x</sub>t + a<sub>y</sub>s + m<sub>y</sub> + e<sub>y</sub>t,
-a<sub>x</sub> - a<sub>y</sub>)* = *((a<sub>x</sub> + a<sub>y</sub>)s +
(m<sub>x</sub> - m<sub>y</sub>) + (e<sub>x</sub> + e<sub>y</sub>)t,
-(a<sub>x</sub> + a<sub>y</sub>))*.

If we let *a<sub>z</sub>* = *a<sub>x</sub> + a<sub>y</sub>* and
*m<sub>z</sub>* = *m<sub>x</sub> + m<sub>y</sub>* and *e<sub>z</sub>* =
*e<sub>x</sub> + e<sub>y</sub>*, the resulting ciphertext is *(a<sub>z</sub>s +
m<sub>z</sub> + e<sub>z</sub>t, -a<sub>z</sub>)*, which is just another
ciphertext encrypting the sum of the original messages.

### Homomorphic Absorption

We can multiply a ciphertext containing the message *m* by another unencrypted
polynomial *p* to get the encryption of the message *mp*. To do so, we simply
multiply the components of the ciphertext by *p*: *(c<sub>x</sub>p,
c<sub>1</sub>p)* = *(aps + mp + ept, -ap)*, which is simply a ciphertext
containing the message *mp* with error *ep* and a value of *a* that has been
changed to *ap*.

### Homomorphic Multiplication

Adding homomorphic multiplication to this library requires small tweaks to the
aforementioned operations.

To multiply two ciphertexts *(c<sub>0x</sub>, c<sub>1x</sub>)* and
*(c<sub>0y</sub>, c<sub>1y</sub>)*, compute their cross product:
*(c<sub>0x</sub>c<sub>0y</sub>, c<sub>1x</sub>c<sub>0y</sub> +
c<sub>0x</sub>c<sub>1y</sub>, c<sub>1x</sub>c<sub>1y</sub>)*.

Notice that the resulting ciphertext has three components, not the usual two.
This is perfectly acceptable and all the aforementioned operations will work as
before with small modifications:

*   Decryption entails taking the inner product of the vector of ciphertext
    polynomials and the vector of powers of the secret key *(s<sup>0</sup>,
    s<sup>1</sup>, s<sup>2</sup>, ...)* and reducing the result (mod *t*).

*   Addition entails adding the ciphertext vectors component-wise. If one vector
    is shorter than the other, simply extend it with polynomials with all-zero
    coefficients.

*   Absorption entails multiplying the plaintext polynomial *p* by each
    component of the ciphertext vector.

### Error Growth

Observe that, after each homomorphic operation, the error in the resulting
ciphertext grows. When adding, it grows additively, where in absorption and
multiplication it grows exponentially. When the largest coefficient of
polynomial that results from taking the inner product of the ciphertext vector
*(c<sub>0</sub>, c<sub>1</sub>, ...)* and the vector of powers of the secret key
*(s<sup>0</sup>, s<sup>1</sup>, ...)* surpasses *q/2*, too much error has built
up and decryption will fail.

As you choose the parameters for your particular application, you will need to
trace the growth of the sum of the error and the message carefully.

*   Addition: The error+message grows additively. Given two ciphertext with
    error+message sums *m<sub>x</sub> + e<sub>x</sub>t* and *m<sub>y</sub> +
    e<sub>y</sub>t*, the result of homomorphically adding these messages will
    have error *m<sub>x</sub> + e<sub>x</sub>t + m<sub>y</sub> +
    e<sub>y</sub>t*.

*   Absorption: The error+message of the result is equal to the product of *(m +
    et)* from the original ciphertext and *p*, the plaintext with which it is
    multiplied, and the number of coefficients in the polynomials *n*. In other
    words, the resulting error is *np(m + et)*. The factor of *n* results from
    the fact that we perform a polynomial multiplication between *p* and *(m +
    et)*.

*   Multiplication: Multiplying two ciphertext has an error equal to the product
    of their error+message values times *n* for the same reasons as listed in
    absorption.

One way of managing error is to set a larger modulus. Setting a larger modulus
must be done with care, however. Security of RLWE derives partly from the ratio
of modulus to error. If the modulus increases in size, so must the standard
deviation of the error distirbution.

### Modulus Switching

Modulus switching is a strategy to slow the rate of error growth when performing
many levels of homomorphic multiplications. The explanation and implementation
of this technique in this library derives from [Fully Homomorphic Encryption
without Bootstrapping](https://eprint.iacr.org/2011/277.pdf).

Modulus switching takes a ciphertext converts a ciphertext from a larger modulus
to a smaller modulus. Specifically, when converting from modulus *q* to modulus
*r*, each coefficient of the ciphertext *k<sub>q</sub>* is multiplied by *r*,
divided by *q*, and rounded to the nearest integer, producing *k<sub>r</sub>*.
This value is then shifted up or down the minimum amount to ensure that
*k<sub>r</sub>* = *k<sub>q</sub>* (mod *t*). This shifting ensures that the
ciphertext will decrypt properly. When modulus switching, the error will scale
with the shift in the modulus, keeping the modulus-to-error ratio constant.

To understand the benefit of modulus switching, consider the following example.
Suppose four ciphertexts initially each have error *e*. After homomorphically
multiplying them in pairs, we will have two ciphertexts each with error
*O(e<sup>2</sup>)*. When multiplying those two ciphertexts, the resulting error
will be *O(e<sup>4</sup>)*. In other words, when multiplying ciphertexts
together, the exponent of the error will grow exponentially.

Modulus switching offers a way to reduce the complexity of the error growth.
After computing two ciphertexts each with error *x<sup>2</sup>*, we shift from
modulus *q* to modulus *q/x*. Doing so reduces the error of each ciphertext to
*x*. The modulus-to-error ratio is the same as before. Were we to convert back
to modulus *q*, the error would return to its original value. When we multiply
these ciphertexts again, the overall error will be *x<sup>2</sup>* (mod *q/x*).
Were we to convert back to the original modulus, the overall error would be
*x<sup>3</sup>*, dramatically smaller than without modulus switching.

This library has the ability to perform modulus switching.

## This Library

This library consists of four major components that form a RLWE stack.

### Montgomery Integers

This library is implemented in `montgomery.(cch|h)`. At the lowest level is a 
library that represents modular integers in Montgomery form, which speeds up the
repeated use of the modulo operator. This library supports 128-bit integers,
meaning that it can support a modulus of up to 126 bits. For larger modulus
sizes, the higher levels of the stack can be parameterized with a different type
(such as a BigInteger). Montgomery integers require several parameters in
addition to the modulus to perform the modular operations efficiently. These
values have been chosen for several common moduli in `constants.h`.

### NTT Polynomial

This library is implemented in `polynomial.h`. We store all polynomials in
NTT (Number-Theoretic Transformation) form. A polynomial multiplication on
standard polynomials can be computed with a coefficient-wise product on the same
polynomials in NTT form, reducing the complexity of the operation from
*O(n<sup>2</sup>)* to *O(n)*.

### NTT Parameters

This library is implemented in `ntt_parameters.(cc|h)`. Converting to and from
NTT form requires special parameters that this file generates. In production
code, these parameters should be generated ahead of time for the chosen values
of *q* and *n* and compiled as part of the final binary.

### Symmetric Encryption.

This library is implemented in `symmetric_encryption.h`. This library implements
all of the cryptographic operations described earlier in ths document. It
operates on polynomials in NTT form.

## Dependencies

This library requires the following external dependencies:

*   [Abseil](https://github.com/abseil/abseil-cpp) for C++ common libraries.

*   [Bazel](https://github.com/bazelbuild/bazel) for building the library.

*   [BoringSSL](https://github.com/google/boringssl) for underlying
    cryptographic operations.

*   [GFlag](https://github.com/gflags/gflags) for flags. Needed to use glog.

*   [GLog](https://github.com/google/glog) for logging.

*   [Google Test](https://github.com/google/googletest) for unit testing the
    library.

*   [Protocol Buffers](https://github.com/google/protobuf) for data
    serialization.

## How to Build

In order to run the SHELL library, you need to install Bazel, if you don't have
it already.
[Follow the instructions for your platform on the Bazel website.](https://docs.bazel.build/versions/master/install.html)

You also need to install Git, if you don't have it already.
[Follow the instructions for your platform on the Git website.](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git)

Once you've installed Bazel and Git, open a Terminal and clone the SHELL
repository into a local folder:

```shell
git clone https://github.com/google/shell-encryption.git
```

Navigate into the `shell-encryption` folder you just created, and build the
SHELL library and dependencies using Bazel. Note, the library must be built
using C++17.

```bash
cd shell-encryption
bazel build :all --cxxopt='-std=c++17'
```

You may also run all tests (recursively) using the following command:

```bash
bazel test ... --cxxopt='-std=c++17'
```

If you get an error, you may need to build/test with the following flags:

```bash
bazel build :all --cxxopt='-std=c++17' --incompatible_disable_deprecated_attr_params=false --incompatible_depset_is_not_iterable=false --incompatible_new_actions_api=false --incompatible_no_support_tools_in_action_inputs=false
```

## Acknowledgements

We also thank Jonathan Frankle, who contributed to this library during an
internship at Google.
