import { Calendar } from "icu4x"
import { Date } from "icu4x"
import { DateTimeFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
import { Time } from "icu4x"
export function formatIso(dateTimeFormatterLocaleName, dateTimeFormatterLength, dateTimeFormatterTimePrecision, dateTimeFormatterAlignment, dateTimeFormatterYearStyle, dateYear, dateMonth, dateDay, timeHour, timeMinute, timeSecond, timeSubsecond) {
    
    let dateTimeFormatterLocale = Locale.fromString(dateTimeFormatterLocaleName);
    
    let dateTimeFormatter = DateTimeFormatter.createYmdt(dateTimeFormatterLocale,dateTimeFormatterLength,dateTimeFormatterTimePrecision,dateTimeFormatterAlignment,dateTimeFormatterYearStyle);
    
    let date = new IsoDate(dateYear,dateMonth,dateDay);
    
    let time = new Time(timeHour,timeMinute,timeSecond,timeSubsecond);
    
    let out = dateTimeFormatter.formatIso(date,time);
    

    return out;
}
export function formatSameCalendar(dateTimeFormatterLocaleName, dateTimeFormatterLength, dateTimeFormatterTimePrecision, dateTimeFormatterAlignment, dateTimeFormatterYearStyle, dateYear, dateMonth, dateDay, dateCalendarLocaleName, timeHour, timeMinute, timeSecond, timeSubsecond) {
    
    let dateTimeFormatterLocale = Locale.fromString(dateTimeFormatterLocaleName);
    
    let dateTimeFormatter = DateTimeFormatter.createYmdt(dateTimeFormatterLocale,dateTimeFormatterLength,dateTimeFormatterTimePrecision,dateTimeFormatterAlignment,dateTimeFormatterYearStyle);
    
    let dateCalendarLocale = Locale.fromString(dateCalendarLocaleName);
    
    let dateCalendar = Calendar.createForLocale(dateCalendarLocale);
    
    let date = Date.fromIsoInCalendar(dateYear,dateMonth,dateDay,dateCalendar);
    
    let time = new Time(timeHour,timeMinute,timeSecond,timeSubsecond);
    
    let out = dateTimeFormatter.formatSameCalendar(date,time);
    

    return out;
}
