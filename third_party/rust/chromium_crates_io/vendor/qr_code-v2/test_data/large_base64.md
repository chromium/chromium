# Notes about `large_base64.in`

Benchmark of base64 input seems like a useful data point because:
* base64 data is a mixture of digits (`Mode::Numeric`), uppercase letters
  (`Mode::Alphanumeric`), and lowercase letters (`Mode::Byte`) and therefore
  should provide fair coverage of parsing and optimization code
* base64 data is somewhat realistic - e.g. one common scenario for QR codes
  is encoding arbitrary URLs and base64 may appear in query part of URLs.

The specific base64 input data below has been taken from
https://cryptopals.com/sets/1/challenges/6 and truncated to 2880 characters
(roughly the upper capacity of `Mode::Byte` segments in version 40, `EcLevel::L`
according to https://www.qrcode.com/en/about/version.html).
