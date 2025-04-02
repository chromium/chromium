import { GregorianDateFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
export function formatIso(gregorianDateFormatterLocaleName, gregorianDateFormatterLength, valueYear, valueMonth, valueDay) {
    
    let gregorianDateFormatterLocale = Locale.fromString(gregorianDateFormatterLocaleName);
    
    let gregorianDateFormatter = GregorianDateFormatter.createWithLength(gregorianDateFormatterLocale,gregorianDateFormatterLength);
    
    let value = new IsoDate(valueYear,valueMonth,valueDay);
    
    let out = gregorianDateFormatter.formatIso(value);
    

    return out;
}
