export * as lib from "icu4x";
import * as CaseMapperDemo from "./CaseMapper.mjs";
export * as CaseMapperDemo from "./CaseMapper.mjs";
import * as TitlecaseMapperDemo from "./TitlecaseMapper.mjs";
export * as TitlecaseMapperDemo from "./TitlecaseMapper.mjs";
import * as DateDemo from "./Date.mjs";
export * as DateDemo from "./Date.mjs";
import * as DateFormatterDemo from "./DateFormatter.mjs";
export * as DateFormatterDemo from "./DateFormatter.mjs";
import * as GregorianDateFormatterDemo from "./GregorianDateFormatter.mjs";
export * as GregorianDateFormatterDemo from "./GregorianDateFormatter.mjs";
import * as NoCalendarFormatterDemo from "./NoCalendarFormatter.mjs";
export * as NoCalendarFormatterDemo from "./NoCalendarFormatter.mjs";
import * as DecimalFormatterDemo from "./DecimalFormatter.mjs";
export * as DecimalFormatterDemo from "./DecimalFormatter.mjs";
import * as DecimalDemo from "./Decimal.mjs";
export * as DecimalDemo from "./Decimal.mjs";
import * as ListFormatterDemo from "./ListFormatter.mjs";
export * as ListFormatterDemo from "./ListFormatter.mjs";
import * as LocaleDemo from "./Locale.mjs";
export * as LocaleDemo from "./Locale.mjs";
import * as DateTimeFormatterDemo from "./DateTimeFormatter.mjs";
export * as DateTimeFormatterDemo from "./DateTimeFormatter.mjs";
import * as DateTimeFormatterGregorianDemo from "./DateTimeFormatterGregorian.mjs";
export * as DateTimeFormatterGregorianDemo from "./DateTimeFormatterGregorian.mjs";
import * as ComposingNormalizerDemo from "./ComposingNormalizer.mjs";
export * as ComposingNormalizerDemo from "./ComposingNormalizer.mjs";
import * as DecomposingNormalizerDemo from "./DecomposingNormalizer.mjs";
export * as DecomposingNormalizerDemo from "./DecomposingNormalizer.mjs";
import * as GregorianZonedDateTimeFormatterDemo from "./GregorianZonedDateTimeFormatter.mjs";
export * as GregorianZonedDateTimeFormatterDemo from "./GregorianZonedDateTimeFormatter.mjs";
import * as ZonedDateTimeFormatterDemo from "./ZonedDateTimeFormatter.mjs";
export * as ZonedDateTimeFormatterDemo from "./ZonedDateTimeFormatter.mjs";
import * as AnyCalendarKindDemo from "./AnyCalendarKind.mjs";
export * as AnyCalendarKindDemo from "./AnyCalendarKind.mjs";

import RenderTerminiWordSegmenter from "./WordSegmenter.mjs";


let termini = Object.assign({
    "CaseMapper.lowercase": {
        func: CaseMapperDemo.lowercase,
        // For avoiding webpacking minifying issues:
        funcName: "CaseMapper.lowercase",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "CaseMapper.uppercase": {
        func: CaseMapperDemo.uppercase,
        // For avoiding webpacking minifying issues:
        funcName: "CaseMapper.uppercase",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "CaseMapper.titlecaseSegmentWithOnlyCaseData": {
        func: CaseMapperDemo.titlecaseSegmentWithOnlyCaseData,
        // For avoiding webpacking minifying issues:
        funcName: "CaseMapper.titlecaseSegmentWithOnlyCaseData",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Options:LeadingAdjustment",
                type: "LeadingAdjustment",
                typeUse: "enumerator"
            },
            
            {
                name: "Options:TrailingCase",
                type: "TrailingCase",
                typeUse: "enumerator"
            }
            
        ]
    },

    "CaseMapper.fold": {
        func: CaseMapperDemo.fold,
        // For avoiding webpacking minifying issues:
        funcName: "CaseMapper.fold",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "CaseMapper.foldTurkic": {
        func: CaseMapperDemo.foldTurkic,
        // For avoiding webpacking minifying issues:
        funcName: "CaseMapper.foldTurkic",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "TitlecaseMapper.titlecaseSegment": {
        func: TitlecaseMapperDemo.titlecaseSegment,
        // For avoiding webpacking minifying issues:
        funcName: "TitlecaseMapper.titlecaseSegment",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Options:LeadingAdjustment",
                type: "LeadingAdjustment",
                typeUse: "enumerator"
            },
            
            {
                name: "Options:TrailingCase",
                type: "TrailingCase",
                typeUse: "enumerator"
            }
            
        ]
    },

    "Date.monthCode": {
        func: DateDemo.monthCode,
        // For avoiding webpacking minifying issues:
        funcName: "Date.monthCode",
        parameters: [
            
            {
                name: "Date:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Calendar:Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "Date.era": {
        func: DateDemo.era,
        // For avoiding webpacking minifying issues:
        funcName: "Date.era",
        parameters: [
            
            {
                name: "Date:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Calendar:Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "DateFormatter.format": {
        func: DateFormatterDemo.format,
        // For avoiding webpacking minifying issues:
        funcName: "DateFormatter.format",
        parameters: [
            
            {
                name: "DateFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "DateFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "Value:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Calendar:Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "DateFormatter.formatIso": {
        func: DateFormatterDemo.formatIso,
        // For avoiding webpacking minifying issues:
        funcName: "DateFormatter.formatIso",
        parameters: [
            
            {
                name: "DateFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "DateFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "Value:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Day",
                type: "number",
                typeUse: "number"
            }
            
        ]
    },

    "GregorianDateFormatter.formatIso": {
        func: GregorianDateFormatterDemo.formatIso,
        // For avoiding webpacking minifying issues:
        funcName: "GregorianDateFormatter.formatIso",
        parameters: [
            
            {
                name: "GregorianDateFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "GregorianDateFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "Value:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Day",
                type: "number",
                typeUse: "number"
            }
            
        ]
    },

    "NoCalendarFormatter.format": {
        func: NoCalendarFormatterDemo.format,
        // For avoiding webpacking minifying issues:
        funcName: "NoCalendarFormatter.format",
        parameters: [
            
            {
                name: "NoCalendarFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "NoCalendarFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "Value:Hour",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Minute",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Second",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Subsecond",
                type: "number",
                typeUse: "number"
            }
            
        ]
    },

    "DecimalFormatter.format": {
        func: DecimalFormatterDemo.format,
        // For avoiding webpacking minifying issues:
        funcName: "DecimalFormatter.format",
        parameters: [
            
            {
                name: "DecimalFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "DecimalFormatter:GroupingStrategy",
                type: "DecimalGroupingStrategy",
                typeUse: "enumerator"
            },
            
            {
                name: "Value:F",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Value:Magnitude",
                type: "number",
                typeUse: "number"
            }
            
        ]
    },

    "Decimal.toString": {
        func: DecimalDemo.toString,
        // For avoiding webpacking minifying issues:
        funcName: "Decimal.toString",
        parameters: [
            
            {
                name: "Decimal:F",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Decimal:Magnitude",
                type: "number",
                typeUse: "number"
            }
            
        ]
    },

    "ListFormatter.format": {
        func: ListFormatterDemo.format,
        // For avoiding webpacking minifying issues:
        funcName: "ListFormatter.format",
        parameters: [
            
            {
                name: "ListFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "ListFormatter:Length",
                type: "ListLength",
                typeUse: "enumerator"
            },
            
            {
                name: "List",
                type: "Array<string>",
                typeUse: "Array<string>"
            }
            
        ]
    },

    "Locale.basename": {
        func: LocaleDemo.basename,
        // For avoiding webpacking minifying issues:
        funcName: "Locale.basename",
        parameters: [
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "Locale.getUnicodeExtension": {
        func: LocaleDemo.getUnicodeExtension,
        // For avoiding webpacking minifying issues:
        funcName: "Locale.getUnicodeExtension",
        parameters: [
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "Locale.language": {
        func: LocaleDemo.language,
        // For avoiding webpacking minifying issues:
        funcName: "Locale.language",
        parameters: [
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "Locale.region": {
        func: LocaleDemo.region,
        // For avoiding webpacking minifying issues:
        funcName: "Locale.region",
        parameters: [
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "Locale.script": {
        func: LocaleDemo.script,
        // For avoiding webpacking minifying issues:
        funcName: "Locale.script",
        parameters: [
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "Locale.normalize": {
        func: LocaleDemo.normalize,
        // For avoiding webpacking minifying issues:
        funcName: "Locale.normalize",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "Locale.toString": {
        func: LocaleDemo.toString,
        // For avoiding webpacking minifying issues:
        funcName: "Locale.toString",
        parameters: [
            
            {
                name: "Locale:Name",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "DateTimeFormatter.formatIso": {
        func: DateTimeFormatterDemo.formatIso,
        // For avoiding webpacking minifying issues:
        funcName: "DateTimeFormatter.formatIso",
        parameters: [
            
            {
                name: "DateTimeFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "DateTimeFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatter:TimePrecision",
                type: "TimePrecision",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatter:Alignment",
                type: "DateTimeAlignment",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatter:YearStyle",
                type: "YearStyle",
                typeUse: "enumerator"
            },
            
            {
                name: "Date:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Hour",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Minute",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Second",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Subsecond",
                type: "number",
                typeUse: "number"
            }
            
        ]
    },

    "DateTimeFormatter.formatSameCalendar": {
        func: DateTimeFormatterDemo.formatSameCalendar,
        // For avoiding webpacking minifying issues:
        funcName: "DateTimeFormatter.formatSameCalendar",
        parameters: [
            
            {
                name: "DateTimeFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "DateTimeFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatter:TimePrecision",
                type: "TimePrecision",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatter:Alignment",
                type: "DateTimeAlignment",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatter:YearStyle",
                type: "YearStyle",
                typeUse: "enumerator"
            },
            
            {
                name: "Date:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Calendar:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Time:Hour",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Minute",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Second",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Subsecond",
                type: "number",
                typeUse: "number"
            }
            
        ]
    },

    "DateTimeFormatterGregorian.formatIso": {
        func: DateTimeFormatterGregorianDemo.formatIso,
        // For avoiding webpacking minifying issues:
        funcName: "DateTimeFormatterGregorian.formatIso",
        parameters: [
            
            {
                name: "DateTimeFormatterGregorian:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "DateTimeFormatterGregorian:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatterGregorian:TimePrecision",
                type: "TimePrecision",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatterGregorian:Alignment",
                type: "DateTimeAlignment",
                typeUse: "enumerator"
            },
            
            {
                name: "DateTimeFormatterGregorian:YearStyle",
                type: "YearStyle",
                typeUse: "enumerator"
            },
            
            {
                name: "Date:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Hour",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Minute",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Second",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Subsecond",
                type: "number",
                typeUse: "number"
            }
            
        ]
    },

    "ComposingNormalizer.normalize": {
        func: ComposingNormalizerDemo.normalize,
        // For avoiding webpacking minifying issues:
        funcName: "ComposingNormalizer.normalize",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "DecomposingNormalizer.normalize": {
        func: DecomposingNormalizerDemo.normalize,
        // For avoiding webpacking minifying issues:
        funcName: "DecomposingNormalizer.normalize",
        parameters: [
            
            {
                name: "S",
                type: "string",
                typeUse: "string"
            }
            
        ]
    },

    "GregorianZonedDateTimeFormatter.formatIso": {
        func: GregorianZonedDateTimeFormatterDemo.formatIso,
        // For avoiding webpacking minifying issues:
        funcName: "GregorianZonedDateTimeFormatter.formatIso",
        parameters: [
            
            {
                name: "GregorianZonedDateTimeFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "GregorianZonedDateTimeFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "Date:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Hour",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Minute",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Second",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Subsecond",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Zone:TimeZoneId:Id",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Zone:Offset:Offset",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Zone:ZoneVariant",
                type: "TimeZoneVariant",
                typeUse: "enumerator"
            }
            
        ]
    },

    "ZonedDateTimeFormatter.format": {
        func: ZonedDateTimeFormatterDemo.format,
        // For avoiding webpacking minifying issues:
        funcName: "ZonedDateTimeFormatter.format",
        parameters: [
            
            {
                name: "ZonedDateTimeFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "ZonedDateTimeFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "Date:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Calendar:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Time:Hour",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Minute",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Second",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Subsecond",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Zone:TimeZoneId:Id",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Zone:Offset:Offset",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Zone:ZoneVariant",
                type: "TimeZoneVariant",
                typeUse: "enumerator"
            }
            
        ]
    },

    "ZonedDateTimeFormatter.formatIso": {
        func: ZonedDateTimeFormatterDemo.formatIso,
        // For avoiding webpacking minifying issues:
        funcName: "ZonedDateTimeFormatter.formatIso",
        parameters: [
            
            {
                name: "ZonedDateTimeFormatter:Locale:Name",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "ZonedDateTimeFormatter:Length",
                type: "DateTimeLength",
                typeUse: "enumerator"
            },
            
            {
                name: "Date:Year",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Month",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Date:Day",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Hour",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Minute",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Second",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Time:Subsecond",
                type: "number",
                typeUse: "number"
            },
            
            {
                name: "Zone:TimeZoneId:Id",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Zone:Offset:Offset",
                type: "string",
                typeUse: "string"
            },
            
            {
                name: "Zone:ZoneVariant",
                type: "TimeZoneVariant",
                typeUse: "enumerator"
            }
            
        ]
    },

    "AnyCalendarKind.bcp47": {
        func: AnyCalendarKindDemo.bcp47,
        // For avoiding webpacking minifying issues:
        funcName: "AnyCalendarKind.bcp47",
        parameters: [
            
            {
                name: "AnyCalendarKind",
                type: "AnyCalendarKind",
                typeUse: "enumerator"
            }
            
        ]
    }
}, RenderTerminiWordSegmenter);

export const RenderInfo = {
    "termini": termini
};