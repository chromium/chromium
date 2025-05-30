jiff-tzdb
=========
This is an optional dependency of `jiff` that embeds the entire [IANA Time Zone
Database] into the compiled binary. Specifically, it embeds the binary [TZif]
for each time zone.

This is most typically used on Windows where there is no standard location
for a system copy of the Time Zone Database. On macOS and Linux, one must
explicitly opt into using `jiff-tzdb`. By default, in Unix environments, Jiff
will look for a system copy of the Time Zone Database.

[IANA Time Zone Database]: https://www.iana.org/time-zones
[TZif]: https://datatracker.ietf.org/doc/html/rfc8536

### Documentation

https://docs.rs/jiff-tzdb
