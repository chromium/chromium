import { Calendar } from "icu4x"
import { Date } from "icu4x"
import { Locale } from "icu4x"
export function monthCode(dateYear, dateMonth, dateDay, dateCalendarLocaleName) {
    
    let dateCalendarLocale = Locale.fromString(dateCalendarLocaleName);
    
    let dateCalendar = Calendar.createForLocale(dateCalendarLocale);
    
    let date = Date.fromIsoInCalendar(dateYear,dateMonth,dateDay,dateCalendar);
    
    let out = date.monthCode;
    

    return out;
}
export function era(dateYear, dateMonth, dateDay, dateCalendarLocaleName) {
    
    let dateCalendarLocale = Locale.fromString(dateCalendarLocaleName);
    
    let dateCalendar = Calendar.createForLocale(dateCalendarLocale);
    
    let date = Date.fromIsoInCalendar(dateYear,dateMonth,dateDay,dateCalendar);
    
    let out = date.era;
    

    return out;
}
