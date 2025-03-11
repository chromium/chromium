import { DateTimeFormatterGregorian } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
import { Time } from "icu4x"
export function formatIso(dateTimeFormatterGregorianLocaleName, dateTimeFormatterGregorianLength, dateTimeFormatterGregorianTimePrecision, dateTimeFormatterGregorianAlignment, dateTimeFormatterGregorianYearStyle, dateYear, dateMonth, dateDay, timeHour, timeMinute, timeSecond, timeSubsecond) {
    
    let dateTimeFormatterGregorianLocale = Locale.fromString(dateTimeFormatterGregorianLocaleName);
    
    let dateTimeFormatterGregorian = DateTimeFormatterGregorian.createYmdt(dateTimeFormatterGregorianLocale,dateTimeFormatterGregorianLength,dateTimeFormatterGregorianTimePrecision,dateTimeFormatterGregorianAlignment,dateTimeFormatterGregorianYearStyle);
    
    let date = new IsoDate(dateYear,dateMonth,dateDay);
    
    let time = new Time(timeHour,timeMinute,timeSecond,timeSubsecond);
    
    let out = dateTimeFormatterGregorian.formatIso(date,time);
    

    return out;
}
