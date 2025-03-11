import { CaseMapper } from "icu4x"
import { Locale } from "icu4x"
import { TitlecaseOptions } from "icu4x"
export function lowercase(s: string, localeName: string);
export function uppercase(s: string, localeName: string);
export function titlecaseSegmentWithOnlyCaseData(s: string, localeName: string, optionsLeadingAdjustment: LeadingAdjustment, optionsTrailingCase: TrailingCase);
export function fold(s: string);
export function foldTurkic(s: string);
