"""Provides an implementation of `hash_to_field` as defined in https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hash-to-curve-07#section-5.

The `hash_to_field` function hashes a byte string of any length into one or more
elements of a field F. This function works in two steps: it first hashes the
input byte string to produce a pseudorandom byte string, and then interprets
this pseudorandom byte string as one or more elements of F.

This code is adapted from https://github.com/cfrg/draft-irtf-cfrg-hash-to-curve/blob/main/poc/hash_to_field.py.
"""
from __future__ import annotations

from abc import ABC, abstractmethod
import struct
from typing import Any, List, Union

_as_bytes = lambda x: x if isinstance(x, bytes) else bytes(x, "utf-8")
_strxor = lambda str1, str2: bytes(s1 ^ s2 for (s1, s2) in zip(str1, str2))


def to_hex(octet_string: Union[bytes, str]) -> str:
    """Returns the input string as a hex string.

    If the input is a string, each character is converted to a byte value, and
    the result is converted to hex. If the input is bytes, it is converted to
    hex directly.

    Args:
        octet_string: The input string to convert.

    Raises:
        ValueError: If the input is not a string or bytes.
    """
    if isinstance(octet_string, str):
        return "".join("{:02x}".format(ord(c)) for c in octet_string)
    if not isinstance(octet_string, bytes):
        raise ValueError(f"Input must be a string or bytes.")
    return "".join("{:02x}".format(c) for c in octet_string)


def I2OSP(val: int, length: int) -> bytes:
    """Converts a nonnegative integer to an octet string of a specified length.
    It is defined in RFC 3447, section 4.1.

    Args:
        val: The nonnegative integer to be converted.
        length: The intended length of the resulting octet string.

    Returns:
        An octet string of the specified length.

    Raises:
        ValueError: If `val` is out of bounds.
    """
    val = int(val)
    if val < 0 or val >= (1 << (8 * length)):
        raise ValueError("bad I2OSP call: val=%d length=%d" % (val, length))
    ret = [0] * length
    val_ = val
    for idx in reversed(range(0, length)):
        ret[idx] = val_ & 0xFF
        val_ = val_ >> 8
    ret = struct.pack("=" + "B" * length, *ret)
    assert OS2IP(ret, True) == val
    return ret


def OS2IP(octets: bytes, skip_assert: bool = False) -> int:
    """Converts an octet string to a nonnegative integer. It is defined in RFC
     3447, section 4.2.

    Args:
        octets: The octet string to be converted.
        skip_assert: Whether or not to skip a check for internal correctness.

    Returns:
        A nonnegative integer.
    """
    ret = 0
    for octet in struct.unpack("=" + "B" * len(octets), octets):
        ret <<= 8
        ret += octet
    if not skip_assert:
        assert octets == I2OSP(ret, len(octets))
    return ret


def hash_to_field(
        msg: Union[bytes, str],
        count: int,
        modulus: int,
        degree: int,
        blen: int,
        expander: Expander,
) -> List[List[int]]:
    """Hashes a byte string of any length into one or more elements of a field
    F. It is defined in https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hash-to-curve-07#section-5

    Args:
        msg: A byte string containing the message to hash.
        count: The number of elements of F to output.
        modulus: The modulus of F.
        degree: The extension degree of F.
        blen: The number of bytes for `expand_message` to produce.
        expander: An instance of `Expander`.

    Returns:
        `(u_0, ..., u_(count - 1))`, a list of field elements.
    """
    len_in_bytes = count * degree * blen
    uniform_bytes = expander.expand_message(msg, len_in_bytes)
    u_vals = []
    for i in range(count):
        e_vals = []
        for j in range(degree):
            elm_offset = blen * (j + i * degree)
            tv = uniform_bytes[elm_offset:(elm_offset + blen)]
            e_vals.append(OS2IP(tv) % modulus)
        u_vals.append(e_vals)
    return u_vals


def expand_message_xmd(
        msg: Union[bytes, str],
        dst: bytes,
        len_in_bytes: int,
        hash_fn: Any,
        security_param: int,
) -> bytes:
    """Produces a pseudorandom byte string using a cryptographic hash function H
    that outputs b bits.

    It is recommended to use SHA-2 or SHA-3 for H. See https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hash-to-curve-07#section-5.3.1
    for more details.

    Args:
        msg: A byte string containing the message to hash.
        dst: A byte string that acts as a domain separation tag.
        len_in_bytes: The length of the requested output in bytes.
        hash_fn: A hash function.
        security_param: The target security level in bits.

    Returns:
        A pseudorandom byte string.

    Raises:
        ValueError: If any of the arguments are invalid.
    """
    # sanity checks and basic parameters
    b_in_bytes = hash_fn().digest_size
    r_in_bytes = hash_fn().block_size
    if not 8 * b_in_bytes >= 2 * security_param:
        raise ValueError("The number of bits output by H MUST be b >= 2 * k.")
    if len(dst) > 255:
        raise ValueError("DST len should be at most 255 bytes")

    # compute ell and check that sizes are as we expect
    ell = (len_in_bytes + b_in_bytes - 1) // b_in_bytes
    if ell > 255:
        raise ValueError("Bad expand_message_xmd call: ell was %d" % ell)

    # compute prefix-free encoding of DST
    dst_prime = dst + I2OSP(len(dst), 1)
    assert len(dst_prime) == len(dst) + 1

    # padding and length strings
    Z_pad = I2OSP(0, r_in_bytes)
    l_i_b_str = I2OSP(len_in_bytes, 2)

    # compute blocks
    b_vals = [None] * ell
    msg_prime = Z_pad + _as_bytes(msg) + l_i_b_str + I2OSP(0, 1) + dst_prime
    b_0 = hash_fn(msg_prime).digest()
    b_vals[0] = hash_fn(b_0 + I2OSP(1, 1) + dst_prime).digest()
    for i in range(1, ell):
        b_vals[i] = hash_fn(
            _strxor(b_0, b_vals[i - 1]) + I2OSP(i + 1, 1) +
            dst_prime).digest()

    # assemble output
    uniform_bytes = (b"").join(b_vals)
    output = uniform_bytes[0:len_in_bytes]

    return output


class Expander(ABC):
    """A base for classes that implement `expand_message` as defined in https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hash-to-curve-07#section-5.3.

    Attributes:
        name: The name of the expander.
        dst: A byte string that acts as a domain separation tag.
        dst_prime: A post-processed form of `dst`.
        hash_fn: A hash function for the expander.
        security_param: The target security level in bits.
    """

    def __init__(self, name: str, dst: str, dst_prime: bytes, hash_fn: Any,
                 security_param: int):
        self.name = name
        self.dst = dst
        self.dst_prime = dst_prime
        self.hash_fn = hash_fn
        self.security_param = security_param

    @abstractmethod
    def expand_message(self, msg: Union[bytes, str],
                       len_in_bytes: int) -> bytes:
        """Generates a pseudorandom byte string from an input string.

        Args:
            msg: A byte string containing the message to hash.
            len_in_bytes: The number of bytes to be generated.
        """
        pass


class XMDExpander(Expander):
    """Implements `expand_message_xmd` as defined in https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hash-to-curve-07#section-5.3.1.

    Attributes:
        dst: A byte string that acts as a domain separation tag.
        hash_fn: A hash function for the expander.
        security_param: The target security level in bits.
    """

    def __init__(self, dst: str, hash_fn: Any, security_param: int):
        dst_prime = _as_bytes(dst)
        if len(dst_prime) > 255:
            # https://cfrg.github.io/draft-irtf-cfrg-hash-to-curve/draft-irtf-cfrg-hash-to-curve.html#name-using-dsts-longer-than-255-
            dst_prime = hash_fn(
                _as_bytes("H2C-OVERSIZE-DST-") + _as_bytes(dst)).digest()
        else:
            dst_prime = _as_bytes(dst)
        super(XMDExpander, self).__init__("expand_message_xmd", dst, dst_prime,
                                          hash_fn, security_param)

    def expand_message(self, msg: Union[bytes, str],
                       len_in_bytes: int) -> bytes:
        return expand_message_xmd(msg, self.dst_prime, len_in_bytes,
                                  self.hash_fn, self.security_param)
