import { GregorianZonedDateTimeFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
import { Time } from "icu4x"
import { TimeZone } from "icu4x"
import { TimeZoneInfo } from "icu4x"
import { UtcOffset } from "icu4x"
export function formatIso(gregorianZonedDateTimeFormatterLocaleName, gregorianZonedDateTimeFormatterLength, dateYear, dateMonth, dateDay, timeHour, timeMinute, timeSecond, timeSubsecond, zoneTimeZoneIdId, zoneOffsetOffset, zoneZoneVariant) {
    
    let gregorianZonedDateTimeFormatterLocale = Locale.fromString(gregorianZonedDateTimeFormatterLocaleName);
    
    let gregorianZonedDateTimeFormatter = GregorianZonedDateTimeFormatter.createWithLength(gregorianZonedDateTimeFormatterLocale,gregorianZonedDateTimeFormatterLength);
    
    let date = new IsoDate(dateYear,dateMonth,dateDay);
    
    let time = new Time(timeHour,timeMinute,timeSecond,timeSubsecond);
    
    let zoneTimeZoneId = TimeZone.createFromBcp47(zoneTimeZoneIdId);
    
    let zoneOffset = UtcOffset.fromString(zoneOffsetOffset);
    
    let zone = new TimeZoneInfo(zoneTimeZoneId,zoneOffset,zoneZoneVariant);
    
    let out = gregorianZonedDateTimeFormatter.formatIso(date,time,zone);
    

    return out;
}
