# Author: Google
# See the LICENSE file for legal information regarding use of this file.

import os
import six

p = (
    115792089210356248762697446949407573530086143415290314195533631308867097853951)
order = (
    115792089210356248762697446949407573529996955224135760342422259061068512044369)
p256B = 0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b

baseX = 0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296
baseY = 0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5
basePoint = (baseX, baseY)


def _pointAdd(a, b):
    Z1Z1 = (a[2] * a[2]) % p
    Z2Z2 = (b[2] * b[2]) % p
    U1 = (a[0] * Z2Z2) % p
    U2 = (b[0] * Z1Z1) % p
    S1 = (a[1] * b[2] * Z2Z2) % p
    S2 = (b[1] * a[2] * Z1Z1) % p
    if U1 == U2 and S1 == S2:
        return pointDouble(a)
    H = (U2 - U1) % p
    I = (4 * H * H) % p
    J = (H * I) % p
    r = (2 * (S2 - S1)) % p
    V = (U1 * I) % p
    X3 = (r * r - J - 2 * V) % p
    Y3 = (r * (V - X3) - 2 * S1 * J) % p
    Z3 = (((a[2] + b[2]) * (a[2] + b[2]) - Z1Z1 - Z2Z2) * H) % p

    return (X3, Y3, Z3)


def _pointDouble(a):
    delta = (a[2] * a[2]) % p
    gamma = (a[1] * a[1]) % p
    beta = (a[0] * gamma) % p
    alpha = (3 * (a[0] - delta) * (a[0] + delta)) % p
    X3 = (alpha * alpha - 8 * beta) % p
    Z3 = ((a[1] + a[2]) * (a[1] + a[2]) - gamma - delta) % p
    Y3 = (alpha * (4 * beta - X3) - 8 * gamma * gamma) % p

    return (X3, Y3, Z3)


def _square(n):
    return (n * n)


def _modpow(a, n, p):
    if n == 0:
        return 1
    if n == 1:
        return a
    r = _square(_modpow(a, n >> 1, p)) % p
    if n & 1 == 1:
        r = (r * a) % p
    return r


def _scalarMult(k, point):
    accum = (0, 0, 0)
    accumIsInfinity = True
    jacobianPoint = (point[0], point[1], 1)

    for bit in range(255, -1, -1):
        if not accumIsInfinity:
            accum = _pointDouble(accum)

        if (k >> bit) & 1 == 1:
            if accumIsInfinity:
                accum = jacobianPoint
                accumIsInfinity = False
            else:
                accum = _pointAdd(accum, jacobianPoint)

    if accumIsInfinity:
        return (0, 0)

    zInv = _modpow(accum[2], p - 2, p)
    return ((accum[0] * zInv * zInv) % p, (accum[1] * zInv * zInv * zInv) % p)


def _scalarBaseMult(k):
    return _scalarMult(k, basePoint)


def _decodeBigEndian(b):
    # TODO(davidben): Replace with int.from_bytes when removing Python 2.
    return sum([six.indexbytes(b, len(b) - i - 1) << 8 * i
                for i in range(len(b))])


def _encodeBigEndian(n):
    b = bytearray()
    while n != 0:
        b.append(n & 0xff)
        n >>= 8

    if len(b) == 0:
        b.append(0)
    b.reverse()

    return bytes(b)


def _zeroPad(b, length):
    if len(b) < length:
        return (b"\x00" * (length - len(b))) + b
    return b


def _encodePoint(point):
    x = point[0]
    y = point[1]
    if (y * y) % p != (x * x * x - 3 * x + p256B) % p:
        raise "point not on curve"
    return b"\x04" + _zeroPad(_encodeBigEndian(point[0]), 32) + _zeroPad(
        _encodeBigEndian(point[1]), 32)


def _decodePoint(b):
    if len(b) != 1 + 32 + 32 or six.indexbytes(b, 0) != 4:
        raise "invalid encoded ec point"
    x = _decodeBigEndian(b[1:33])
    y = _decodeBigEndian(b[33:65])
    if (y * y) % p != (x * x * x - 3 * x + p256B) % p:
        raise "point not on curve"
    return (x, y)


def generatePublicPrivate():
    """generatePublicPrivate returns a tuple of (X9.62 encoded public point,
    private value), where the private value is generated from os.urandom."""
    private = _decodeBigEndian(os.urandom(40)) % order
    return _encodePoint(_scalarBaseMult(private)), private


def generateSharedValue(theirPublic, private):
    """generateSharedValue returns the encoded x-coordinate of the
    multiplication of a peer's X9.62 encoded point and a private value."""
    return _zeroPad(
        _encodeBigEndian(_scalarMult(private, _decodePoint(theirPublic))[0]),
        32)

if __name__ == "__main__":
    alice, alicePrivate = generatePublicPrivate()
    bob, bobPrivate = generatePublicPrivate()

    if generateSharedValue(alice, bobPrivate) != generateSharedValue(
        bob, alicePrivate):
        raise "simple DH test failed"

    (x, _) = _scalarBaseMult(1)

    for i in range(1000):
        (x, _) = _scalarBaseMult(x)

    if x != 2428281965257598569040586318034812501729437946720808289049534492833635302706:
        raise "loop test failed"
