import { CaseMapper } from "icu4x"
import { DataProvider } from "icu4x"
import { Locale } from "icu4x"
import { TitlecaseOptions } from "icu4x"
export function lowercase(s: string, name: string);
export function uppercase(s: string, name: string);
export function titlecaseSegmentWithOnlyCaseData(s: string, name: string, leading_adjustment: LeadingAdjustment, trailing_case: TrailingCase);
export function fold(s: string);
export function foldTurkic(s: string);
