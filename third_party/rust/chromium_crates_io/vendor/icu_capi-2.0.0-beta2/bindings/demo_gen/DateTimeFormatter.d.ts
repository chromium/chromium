import { Calendar } from "icu4x"
import { Date } from "icu4x"
import { DateTimeFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
import { Time } from "icu4x"
export function formatIso(dateTimeFormatterLocaleName: string, dateTimeFormatterLength: DateTimeLength, dateTimeFormatterTimePrecision: TimePrecision, dateTimeFormatterAlignment: DateTimeAlignment, dateTimeFormatterYearStyle: YearStyle, dateYear: number, dateMonth: number, dateDay: number, timeHour: number, timeMinute: number, timeSecond: number, timeSubsecond: number);
export function formatSameCalendar(dateTimeFormatterLocaleName: string, dateTimeFormatterLength: DateTimeLength, dateTimeFormatterTimePrecision: TimePrecision, dateTimeFormatterAlignment: DateTimeAlignment, dateTimeFormatterYearStyle: YearStyle, dateYear: number, dateMonth: number, dateDay: number, dateCalendarLocaleName: string, timeHour: number, timeMinute: number, timeSecond: number, timeSubsecond: number);
