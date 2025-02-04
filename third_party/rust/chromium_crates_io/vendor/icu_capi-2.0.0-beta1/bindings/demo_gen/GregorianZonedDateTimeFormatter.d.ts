import { DataProvider } from "icu4x"
import { GregorianZonedDateTimeFormatter } from "icu4x"
import { IsoDateTime } from "icu4x"
import { Locale } from "icu4x"
import { TimeZoneInfo } from "icu4x"
export function formatIsoDatetimeWithCustomTimeZone(name: string, length: DateTimeLength, year: number, month: number, day: number, hour: number, minute: number, second: number, nanosecond: number, bcp47Id: string, offsetSeconds: number, dst: boolean);
