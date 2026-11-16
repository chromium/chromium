
//#region src/types.d.ts
type RangeTuple = [number, number];
interface SearchResult {
  isMatch: boolean;
  score: number;
  indices?: ReadonlyArray<RangeTuple>;
  /** @internal Aggregation flag for extended-search inverse terms. */
  hasInverse?: boolean;
  /**
   * @internal Token-search `tokenMatch: 'all'` coverage for this text.
   * `matchedMask` bit `i` ⇒ query term `i` matched here (≤31-term fast path);
   * `matchedTerms` is the equivalent set for the ≥32-term fallback.
   */
  matchedMask?: number;
  /** @internal */
  matchedTerms?: Set<number>;
  /** @internal Query token count; descriptor for the record-level AND gate. */
  termCount?: number;
}
interface Searcher {
  searchIn(text: string): SearchResult;
}
interface FuseOptionKeyObject<T> {
  name: string | string[];
  weight?: number;
  getFn?: (obj: T) => ReadonlyArray<string> | string | null | undefined;
}
type FuseOptionKey<T> = FuseOptionKeyObject<T> | string | string[];
interface KeyObject {
  path: string[];
  id: string;
  weight: number;
  src: string | string[];
  getFn?: ((obj: any) => ReadonlyArray<string> | string | null | undefined) | null;
}
type FuseGetFunction<T> = (obj: T, path: string | string[]) => ReadonlyArray<string> | string;
type GetFunction = (obj: any, path: string | string[]) => any;
/**
 * Custom tokenizer for `useTokenSearch`. Receives the field/query text after
 * case-folding and diacritic-stripping (per `isCaseSensitive` /
 * `ignoreDiacritics`) and must return the term list. Functions must be
 * deterministic — non-deterministic output silently breaks `df` accounting.
 */
type FuseTokenizeFunction = (text: string) => string[];
interface NormInterface {
  get(value: string): number;
  clear(): void;
}
interface RecordEntryObject {
  /** The text value */
  v: string;
  /** The field-length norm */
  n: number;
}
type RecordEntryArrayItem = ReadonlyArray<RecordEntryObject & {
  i: number;
}>;
type RecordEntry = {
  [key: string]: RecordEntryObject | RecordEntryArrayItem;
};
interface FuseIndexObjectRecord {
  /** The index of the record in the source list */
  i: number;
  $: RecordEntry;
}
interface FuseIndexStringRecord {
  /** The index of the record in the source list */
  i: number;
  /** The text value */
  v: string;
  /** The field-length norm */
  n: number;
}
type FuseIndexRecords = ReadonlyArray<FuseIndexObjectRecord> | ReadonlyArray<FuseIndexStringRecord>;
interface IndexRecord {
  i: number;
  v?: string;
  n?: number;
  $?: Record<number, SubRecord | SubRecord[]>;
}
interface SubRecord {
  v: string;
  i?: number;
  n: number;
}
type FuseSortFunctionItem = {
  [key: string]: {
    $: string;
  } | {
    $: string;
    idx: number;
  }[];
};
type FuseSortFunctionMatch = {
  score: number;
  key: KeyObject;
  value: string;
  indices: ReadonlyArray<number>[];
};
type FuseSortFunctionMatchList = FuseSortFunctionMatch & {
  idx: number;
};
type FuseSortFunctionArg = {
  idx: number;
  item: FuseSortFunctionItem;
  score: number;
  matches?: (FuseSortFunctionMatch | FuseSortFunctionMatchList)[];
};
type FuseSortFunction = (a: FuseSortFunctionArg, b: FuseSortFunctionArg) => number;
interface MatchScore {
  score: number;
  key?: KeyObject | null;
  value: string;
  idx?: number;
  hasInverse?: boolean;
  norm: number;
  indices?: ReadonlyArray<RangeTuple>;
  /** @internal Token-search `tokenMatch: 'all'` coverage carried up for record-level gating. */
  matchedMask?: number;
  /** @internal */
  matchedTerms?: Set<number>;
  /** @internal */
  termCount?: number;
}
interface InternalResult {
  idx: number;
  item: any;
  score?: number;
  matches: MatchScore[];
}
interface IFuseOptions<T> {
  /** Indicates whether comparisons should be case sensitive. */
  isCaseSensitive?: boolean;
  /** Indicates whether comparisons should ignore diacritics (accents). */
  ignoreDiacritics?: boolean;
  /** Determines how close the match must be to the fuzzy location. */
  distance?: number;
  /** When true, the matching function will continue to the end of a search pattern even if a perfect match has already been located in the string. */
  findAllMatches?: boolean;
  /** The function to use to retrieve an object's value at the provided path. */
  getFn?: FuseGetFunction<T>;
  /** When `true`, search will ignore `location` and `distance`. */
  ignoreLocation?: boolean;
  /** When `true`, the calculation for the relevance score will ignore the field-length norm. */
  ignoreFieldNorm?: boolean;
  /** Determines how much the field-length norm affects scoring. */
  fieldNormWeight?: number;
  /** Whether the matches should be included in the result set. */
  includeMatches?: boolean;
  /** Whether the score should be included in the result set. */
  includeScore?: boolean;
  /** List of keys that will be searched. */
  keys?: Array<FuseOptionKey<T>>;
  /** Determines approximately where in the text is the pattern expected to be found. */
  location?: number;
  /** Only the matches whose length exceeds this value will be returned. */
  minMatchCharLength?: number;
  /** Whether to sort the result list, by score. */
  shouldSort?: boolean;
  /** The function to use to sort all the results. */
  sortFn?: FuseSortFunction;
  /** At what point does the match algorithm give up. */
  threshold?: number;
  /** When `true`, it enables the use of unix-like search commands. */
  useExtendedSearch?: boolean;
  /** When `true`, enables token search with TF-IDF scoring. */
  useTokenSearch?: boolean;
  /**
   * Tokenizer used by `useTokenSearch`, applied identically at index-build
   * and query time. Accepts either a global `RegExp` or a function returning
   * `string[]`. Defaults to `/[\p{L}\p{M}\p{N}_]+/gu`, which handles CJK,
   * Cyrillic, Greek, Arabic, Hebrew, Devanagari, etc. out of the box. Use a
   * function form (e.g. wrapping `Intl.Segmenter`) for word-segmentation in
   * scripts without whitespace boundaries.
   */
  tokenize?: RegExp | FuseTokenizeFunction;
  /**
   * How the words of a multi-word query combine, for `useTokenSearch` only.
   * `'any'` (default) returns a record if it matches any query word (OR);
   * `'all'` returns it only when every query word matches somewhere in the
   * record — any field or array element (AND). Use `'all'` for filtering,
   * where adding a word should narrow the results. Has no effect unless
   * `useTokenSearch` is `true`.
   */
  tokenMatch?: 'all' | 'any';
}
interface FuseIndexOptions<T> {
  getFn?: FuseGetFunction<T>;
  fieldNormWeight?: number;
}
interface FuseSearchOptions {
  limit?: number;
}
interface FuseResultMatch {
  indices: ReadonlyArray<RangeTuple>;
  key?: string;
  refIndex?: number;
  value?: string;
}
interface FuseResult<T> {
  item: T;
  refIndex: number;
  score?: number;
  matches?: ReadonlyArray<FuseResultMatch>;
}
type Expression = string | {
  [key: string]: string;
} | {
  $path: ReadonlyArray<string>;
  $val: string;
} | {
  $and?: Expression[];
} | {
  $or?: Expression[];
};
//#endregion
//#region src/tools/FuseIndex.d.ts
declare class FuseIndex<T = any> {
  norm: NormInterface;
  getFn: GetFunction;
  isCreated: boolean;
  docs: ReadonlyArray<T>;
  records: IndexRecord[];
  keys: KeyObject[];
  _keysMap: Record<string, number>;
  constructor({
    getFn,
    fieldNormWeight
  }?: FuseIndexOptions<T>);
  setSources(docs?: ReadonlyArray<T>): void;
  setIndexRecords(records?: IndexRecord[]): void;
  setKeys(keys?: KeyObject[]): void;
  create(): void;
  add(doc: T, docIndex: number): IndexRecord | null;
  removeAt(idx: number): void;
  removeAll(indices: number[]): void;
  getValueForItemAtKeyId(item: any, keyId: string): any;
  size(): number;
  _createStringRecord(doc: string, docIndex: number): IndexRecord | null;
  _createObjectRecord(doc: any, docIndex: number): IndexRecord;
  toJSON(): {
    keys: ReadonlyArray<Omit<KeyObject, 'getFn'>>;
    records: IndexRecord[];
  };
}
declare function createIndex<T>(keys: FuseOptionKey<T>[], docs: ReadonlyArray<T>, {
  getFn,
  fieldNormWeight
}?: FuseIndexOptions<T>): FuseIndex<T>;
declare function parseIndex<T>(data: {
  keys: ReadonlyArray<KeyObject>;
  records: IndexRecord[];
}, {
  getFn,
  fieldNormWeight
}?: FuseIndexOptions<T>): FuseIndex<T>;
//#endregion
//#region src/tools/KeyStore.d.ts
declare class KeyStore {
  _keys: KeyObject[];
  _keyMap: Record<string, KeyObject>;
  constructor(keys: FuseOptionKey<any>[]);
  get(keyId: string): KeyObject;
  keys(): KeyObject[];
  toJSON(): string;
}
//#endregion
//#region src/core/queryParser.d.ts
interface ParsedLeaf {
  keyId: string | null;
  pattern: string;
  searcher?: Searcher;
}
interface ParsedOperator {
  children: ParsedNode[];
  operator: string;
}
type ParsedNode = ParsedLeaf | ParsedOperator;
declare function parse(query: Expression, options: any, {
  auto
}?: {
  auto?: boolean | undefined;
}): ParsedNode;
//#endregion
//#region src/core/config.d.ts
declare const Config: Required<IFuseOptions<any>>;
//#endregion
//#region src/tools/MaxHeap.d.ts
type Comparator = (a: InternalResult, b: InternalResult) => number;
declare class MaxHeap {
  limit: number;
  heap: InternalResult[];
  comparator: Comparator;
  constructor(limit: number, comparator: Comparator);
  get size(): number;
  insert(item: InternalResult): void;
  extractSorted(): InternalResult[];
  _bubbleUp(i: number): void;
  _sinkDown(i: number): void;
}
//#endregion
//#region src/search/token/InvertedIndex.d.ts
interface InvertedIndexData {
  fieldCount: number;
  df: Map<string, number>;
  docFieldCount: Map<number, number>;
  docTermFieldHits: Map<number, Map<string, number>>;
}
//#endregion
//#region src/core/index.d.ts
interface HeapSearchOptions {
  heap?: MaxHeap;
  ignoreFieldNorm?: boolean;
}
declare class Fuse<T> {
  options: Required<IFuseOptions<T>>;
  _keyStore: KeyStore;
  _docs: T[];
  _myIndex: FuseIndex<T>;
  _invertedIndex: InvertedIndexData | null;
  _lastQuery: string | null;
  _lastSearcher: Searcher | null;
  static version: string;
  static createIndex: typeof createIndex;
  static parseIndex: typeof parseIndex;
  static config: typeof Config;
  static parseQuery: typeof parse;
  static use: (...plugins: any[]) => void;
  static match: (pattern: string, text: string, options?: IFuseOptions<any>) => SearchResult;
  constructor(docs: ReadonlyArray<T>, options?: IFuseOptions<T>, index?: FuseIndex<T>);
  _getSearcher(query: string): Searcher;
  setCollection(docs: ReadonlyArray<T>, index?: FuseIndex<T>): void;
  add(doc: T): void;
  remove(predicate?: (doc: T, idx: number) => boolean): T[];
  removeAt(idx: number): T;
  _invalidateSearcherCache(): void;
  getIndex(): FuseIndex<T>;
  _normalizedKeys(): KeyObject[];
  search(query: string | Expression, options?: FuseSearchOptions): FuseResult<T>[];
  _searchStringList(query: string, {
    heap,
    ignoreFieldNorm
  }?: HeapSearchOptions): InternalResult[] | null;
  _searchLogical(query: Expression): InternalResult[];
  _searchObjectList(query: string, {
    heap,
    ignoreFieldNorm
  }?: HeapSearchOptions): InternalResult[] | null;
  _findMatches({
    key,
    value,
    searcher
  }: {
    key: KeyObject | null;
    value: SubRecord | SubRecord[] | undefined;
    searcher: Searcher;
  }): MatchScore[];
  _coversAllTokens(matches: MatchScore[]): boolean;
}
//#endregion
export { type Expression, type FuseGetFunction, type FuseIndex, type FuseIndexObjectRecord, type FuseIndexOptions, type FuseIndexRecords, type FuseIndexStringRecord, type FuseOptionKey, type FuseOptionKeyObject, type FuseResult, type FuseResultMatch, type FuseSearchOptions, type FuseSortFunction, type FuseSortFunctionArg, type FuseSortFunctionItem, type FuseSortFunctionMatch, type FuseSortFunctionMatchList, type FuseTokenizeFunction, type IFuseOptions, type RangeTuple, type RecordEntry, type RecordEntryArrayItem, type RecordEntryObject, type SearchResult, Fuse as default };