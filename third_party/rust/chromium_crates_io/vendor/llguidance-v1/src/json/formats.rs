pub fn lookup_format(name: &str) -> Option<&str> {
    let r = match name {
        "date-time" => concat!(
            r"(?P<date>",
            r"[0-9]{4}-(?:",
            r"(?:0[13578]|1[02])-(?:0[1-9]|[12][0-9]|3[01])|", // 31-day months
            r"(?:0[469]|11)-(?:0[1-9]|[12][0-9]|30)|",         // 30-day months
            r"(?:02)-(?:0[1-9]|1[0-9]|2[0-9])",                // February up to 29 days
            r"))",
            r"[tT](?P<time>",
            r"(?:[01][0-9]|2[0-3]):[0-5][0-9]:", // Hours, Minutes
            r"(?:[0-5][0-9]|60)",                // Seconds (including leap second 60)
            r"(?P<time_fraction>\.[0-9]+)?",     // Optional fractional seconds
            r"(?P<time_zone>",
            r"[zZ]|[+-](?:[01][0-9]|2[0-3]):[0-5][0-9]", // Time zone
            r")",
            r")"
        ),
        "time" => {
            r"(?:[01][0-9]|2[0-3]):[0-5][0-9]:(?:[0-5][0-9]|60)(?P<time_fraction>\.[0-9]+)?(?P<time_zone>[zZ]|[+-](?:[01][0-9]|2[0-3]):[0-5][0-9])"
        }
        "date" => concat!(
            r"(?:[0-9]{4}-(?:0[13578]|1[02])-(?:0[1-9]|[12][0-9]|3[01]))|", // Months with 31 days
            r"(?:[0-9]{4}-(?:0[469]|11)-(?:0[1-9]|[12][0-9]|30))|",         // Months with 30 days
            r"(?:[0-9]{4}-02-(?:0[1-9]|1[0-9]|2[0-9]))", // February with up to 29 days
        ),
        "duration" => {
            r"P(?:(?P<dur_date>(?:(?P<dur_year>[0-9]+Y(?:[0-9]+M(?:[0-9]+D)?)?)|(?P<dur_month>[0-9]+M(?:[0-9]+D)?)|(?P<dur_day>[0-9]+D))(?:T(?:(?P<dur_hour>[0-9]+H(?:[0-9]+M(?:[0-9]+S)?)?)|(?P<dur_minute>[0-9]+M(?:[0-9]+S)?)|(?P<dur_second>[0-9]+S)))?)|(?P<dur_time>T(?:(?P<dur_hour2>[0-9]+H(?:[0-9]+M(?:[0-9]+S)?)?)|(?P<dur_minute2>[0-9]+M(?:[0-9]+S)?)|(?P<dur_second2>[0-9]+S)))|(?P<dur_week>[0-9]+W))"
        }
        // https://www.rfc-editor.org/rfc/inline-errata/rfc5321.html 4.1.2 -> Mailbox
        "email" => concat!(
            r"(?P<local_part>(?P<dot_string>[a-zA-Z0-9!#$%&'*+\-/=?\^_`{|}~]+(\.[a-zA-Z0-9!#$%&'*+\-/=?\^_`{|}~]+)*))",
            r"@(",
            r"(?P<domain>(?P<sub_domain>[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?)(\.(?P<sub_domain2>[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?))*)",
            r"|",
            r"\[(?P<ipv4>((([0-9])|(([1-9])[0-9]|(25[0-5]|(2[0-4]|(1)[0-9])[0-9])))\.){3}(([0-9])|(([1-9])[0-9]|(25[0-5]|(2[0-4]|(1)[0-9])[0-9]))))\]",
            r")"
        ),
        "hostname" => {
            r"[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*"
        }
        "ipv4" => {
            r"((([0-9])|(([1-9])[0-9]|(25[0-5]|(2[0-4]|(1)[0-9])[0-9])))\.){3}(([0-9])|(([1-9])[0-9]|(25[0-5]|(2[0-4]|(1)[0-9])[0-9])))"
        }
        "ipv6" => concat!(
            r"(?:(?P<full>(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}))|",
            r"(?:::(?:[0-9a-fA-F]{1,4}:){0,5}(?P<ls32>[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,4}))|",
            r"(?:(?P<h16_1>[0-9a-fA-F]{1,4})?::(?:[0-9a-fA-F]{1,4}:){0,4}(?P<ls32_1>[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,4}))|",
            r"(?:((?:[0-9a-fA-F]{1,4}:){0,1}[0-9a-fA-F]{1,4})?::(?:[0-9a-fA-F]{1,4}:){0,3}(?P<ls32_2>[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,4}))|",
            r"(?:((?:[0-9a-fA-F]{1,4}:){0,2}[0-9a-fA-F]{1,4})?::(?:[0-9a-fA-F]{1,4}:){0,2}(?P<ls32_3>[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,4}))|",
            r"(?:((?:[0-9a-fA-F]{1,4}:){0,3}[0-9a-fA-F]{1,4})?::[0-9a-fA-F]{1,4}:(?P<ls32_4>[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,4}))|",
            r"(?:((?:[0-9a-fA-F]{1,4}:){0,4}[0-9a-fA-F]{1,4})?::(?P<ls32_5>[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,4}))|",
            r"(?:((?:[0-9a-fA-F]{1,4}:){0,5}[0-9a-fA-F]{1,4})?::(?P<h16_2>[0-9a-fA-F]{1,4}))|",
            r"(?:((?:[0-9a-fA-F]{1,4}:){0,6}[0-9a-fA-F]{1,4})?::)",
        ),
        "uuid" => {
            r"(?P<time_low>[0-9a-fA-F]{8})-(?P<time_mid>[0-9a-fA-F]{4})-(?P<time_high_and_version>[0-9a-fA-F]{4})-(?P<clock_seq_and_reserved>[0-9a-fA-F]{2})(?P<clock_seq_low>[0-9a-fA-F]{2})-(?P<node>[0-9a-fA-F]{12})"
        }
        // RFC 3986 URI - strict compliance
        // URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
        "uri" => concat!(
            // scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
            r"(?P<scheme>[a-zA-Z][a-zA-Z0-9+\-.]*)",
            r":",
            // hier-part = "//" authority path-abempty / path-absolute / path-rootless / path-empty
            r"(?:",
            // "//" authority path-abempty
            r"//",
            // authority = [ userinfo "@" ] host [ ":" port ]
            r"(?:",
            // userinfo = *( unreserved / pct-encoded / sub-delims / ":" )
            r"(?P<userinfo>(?:[a-zA-Z0-9\-._~!$&'()*+,;=:]|%[0-9a-fA-F]{2})*)",
            r"@",
            r")?",
            // host = IP-literal / IPv4address / reg-name
            r"(?P<host>",
            // IP-literal = "[" ( IPv6address / IPvFuture ) "]"
            r"\[",
            r"(?:",
            // IPv6address (simplified - covers common forms)
            r"(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}|", // full
            r"(?:[0-9a-fA-F]{1,4}:){1,7}:|",              // with trailing ::
            r"(?:[0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|", // :: with 1 group after
            r"(?:[0-9a-fA-F]{1,4}:){1,5}(?::[0-9a-fA-F]{1,4}){1,2}|", // :: with 2 groups after
            r"(?:[0-9a-fA-F]{1,4}:){1,4}(?::[0-9a-fA-F]{1,4}){1,3}|", // :: with 3 groups after
            r"(?:[0-9a-fA-F]{1,4}:){1,3}(?::[0-9a-fA-F]{1,4}){1,4}|", // :: with 4 groups after
            r"(?:[0-9a-fA-F]{1,4}:){1,2}(?::[0-9a-fA-F]{1,4}){1,5}|", // :: with 5 groups after
            r"[0-9a-fA-F]{1,4}:(?::[0-9a-fA-F]{1,4}){1,6}|", // :: with 6 groups after
            r":(?::[0-9a-fA-F]{1,4}){1,7}|",              // ::x:x:x:x:x:x:x
            r"::|",                                       // :: alone
            // IPvFuture = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
            r"v[0-9a-fA-F]+\.[a-zA-Z0-9\-._~!$&'()*+,;=:]+",
            r")",
            r"\]|",
            // IPv4address
            r"(?:(?:25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])|",
            // reg-name = *( unreserved / pct-encoded / sub-delims )
            // Note: [ and \ are not valid in reg-name, host validation happens at regex level
            r"(?:[a-zA-Z0-9\-._~!$&'()*+,;=]|%[0-9a-fA-F]{2})*",
            r")",
            // [ ":" port ]
            r"(?::(?P<port>[0-9]*))?",
            // path-abempty = *( "/" segment )
            // Disallow invalid characters: \, <, >, {, }, ^, `, |, space, control chars
            r"(?P<path_abempty>(?:/(?:[a-zA-Z0-9\-._~!$&'()*+,;=:@]|%[0-9a-fA-F]{2})*)*)",
            r"|",
            // path-absolute = "/" [ segment-nz *( "/" segment ) ]
            r"(?P<path_absolute>/(?:(?:[a-zA-Z0-9\-._~!$&'()*+,;=:@]|%[0-9a-fA-F]{2})+(?:/(?:[a-zA-Z0-9\-._~!$&'()*+,;=:@]|%[0-9a-fA-F]{2})*)*)?)",
            r"|",
            // path-rootless = segment-nz *( "/" segment )
            r"(?P<path_rootless>(?:[a-zA-Z0-9\-._~!$&'()*+,;=:@]|%[0-9a-fA-F]{2})+(?:/(?:[a-zA-Z0-9\-._~!$&'()*+,;=:@]|%[0-9a-fA-F]{2})*)*)",
            r"|",
            // path-empty = ""
            r"(?P<path_empty>)",
            r")",
            // [ "?" query ]
            r"(?:\?(?P<query>(?:[a-zA-Z0-9\-._~!$&'()*+,;=:@/?]|%[0-9a-fA-F]{2})*))?",
            // [ "#" fragment ]
            r"(?:\#(?P<fragment>(?:[a-zA-Z0-9\-._~!$&'()*+,;=:@/?]|%[0-9a-fA-F]{2})*))?"
        ),
        "unknown" => r"(?s:.*)",
        _ => return None,
    };
    Some(r)
}
