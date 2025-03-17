import { Locale } from "icu4x"
import { NoCalendarFormatter } from "icu4x"
import { Time } from "icu4x"
export function format(noCalendarFormatterLocaleName, noCalendarFormatterLength, valueHour, valueMinute, valueSecond, valueSubsecond) {
    
    let noCalendarFormatterLocale = Locale.fromString(noCalendarFormatterLocaleName);
    
    let noCalendarFormatter = NoCalendarFormatter.createWithLength(noCalendarFormatterLocale,noCalendarFormatterLength);
    
    let value = new Time(valueHour,valueMinute,valueSecond,valueSubsecond);
    
    let out = noCalendarFormatter.format(value);
    

    return out;
}
