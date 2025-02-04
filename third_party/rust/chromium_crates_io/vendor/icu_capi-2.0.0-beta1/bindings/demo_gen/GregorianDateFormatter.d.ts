import { DataProvider } from "icu4x"
import { GregorianDateFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { IsoDateTime } from "icu4x"
import { Locale } from "icu4x"
export function formatIsoDate(name: string, length: DateTimeLength, year: number, month: number, day: number);
export function formatIsoDatetime(name: string, length: DateTimeLength, year: number, month: number, day: number, hour: number, minute: number, second: number, nanosecond: number);
