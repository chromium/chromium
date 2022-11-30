// Type definitions for Fuse.js v6.4.1
// TypeScript v3.9.5

export default Fuse
export as namespace Fuse

declare class Fuse<T> {
  constructor(
    list: ReadonlyArray<T>,
    options?: Fuse.IFuseOptions<T>,
    index?: FuseIndex<T>
  )
  /**
   * Search function for the Fuse instance.
   *
   * ```typescript
   * const list: MyType[] = [myType1, myType2, etc...]

   * const options: Fuse.IFuseOptions<MyType> = {
   *  keys: ['key1', 'key2']
   * }
   *
   * const myFuse = new Fuse(list, options)
   * let result = myFuse.search('pattern')
   * ```
   *
   * @param pattern The pattern to search
   * @param options `Fuse.FuseSearchOptions`
   * @returns An array of search results
   */
  search<R = T>(
    pattern: string | Fuse.Expression,
    options?: Fuse.FuseSearchOptions
  ): Fuse.FuseResult<R>[]

  setCollection(docs: ReadonlyArray<T>, index?: FuseIndex<T>): void

  /**
   * Adds a doc to the end the list.
   */
  add(doc: T): void

  /**
   * Removes all documents from the list which the predicate returns truthy for,
   * and returns an array of the removed docs.
   * The predicate is invoked with two arguments: (doc, index).
   */
  remove(predicate: (doc: T, idx: number) => boolean): T[]

  /**
   * Removes the doc at the specified index.
   */
  removeAt(idx: number): void

  /**
   * Returns the generated Fuse index
   */
  getIndex(): FuseIndex<T>

  /**
   * Return the current version.
   */
  static version: string

  /**
   * Use this method to pre-generate the index from the list, and pass it
   * directly into the Fuse instance.
   *
   * _Note that Fuse will automatically index the table if one isn't provided
   * during instantiation._
   *
   * ```typescript
   * const list: MyType[] = [myType1, myType2, etc...]
   *
   * const index = Fuse.createIndex<MyType>(
   *  keys: ['key1', 'key2']
   *  list: list
   * )
   *
   * const options: Fuse.IFuseOptions<MyType> = {
   *  keys: ['key1', 'key2']
   * }
   *
   * const myFuse = new Fuse(list, options, index)
   * ```
   * @param keys    The keys to index
   * @param list    The list from which to create an index
   * @param options?
   * @returns An indexed list
   */
  static createIndex<U>(
    keys: Array<Fuse.FuseOptionKey>,
    list: ReadonlyArray<U>,
    options?: Fuse.FuseIndexOptions<U>
  ): FuseIndex<U>

  static parseIndex<U>(
    index: any,
    options?: Fuse.FuseIndexOptions<U>
  ): FuseIndex<U>
}

declare class FuseIndex<T> {
  constructor(options?: Fuse.FuseIndexOptions<T>)
  setSources(docs: ReadonlyArray<T>): void
  setKeys(keys: ReadonlyArray<string>): void
  setIndexRecords(records: Fuse.FuseIndexRecords): void
  create(): void
  add(doc: T): void
  toJSON(): {
    keys: ReadonlyArray<string>
    collection: Fuse.FuseIndexRecords
  }
}

declare namespace Fuse {
  type FuseGetFunction<T> = (
    obj: T,
    path: string | string[]
  ) => ReadonlyArray<string> | string

  export type FuseIndexOptions<T> = {
    getFn: FuseGetFunction<T>
  }

  // {
  //   title: { '$': "Old Man's War" },
  //   'author.firstName': { '$': 'Codenar' }
  // }
  //
  // OR
  //
  // {
  //   tags: [
  //     { $: 'nonfiction', idx: 0 },
  //     { $: 'web development', idx: 1 },
  //   ]
  // }
  export type FuseSortFunctionItem = {
    [key: string]: { $: string } | { $: string; idx: number }[]
  }

  // {
  //   score: 0.001,
  //   key: 'author.firstName',
  //   value: 'Codenar',
  //   indices: [ [ 0, 3 ] ]
  // }
  export type FuseSortFunctionMatch = {
    score: number
    key: string
    value: string
    indices: ReadonlyArray<number>[]
  }

  // {
  //   score: 0,
  //   key: 'tags',
  //   value: 'nonfiction',
  //   idx: 1,
  //   indices: [ [ 0, 9 ] ]
  // }
  export type FuseSortFunctionMatchList = FuseSortFunctionMatch & {
    idx: number
  }

  export type FuseSortFunctionArg = {
    idx: number
    item: FuseSortFunctionItem
    score: number
    matches?: (FuseSortFunctionMatch | FuseSortFunctionMatchList)[]
  }

  export type FuseSortFunction = (
    a: FuseSortFunctionArg,
    b: FuseSortFunctionArg
  ) => number

  // title: {
  //   '$': "Old Man's War",
  //   'n': 0.5773502691896258
  // }
  type RecordEntryObject = {
    v: string // The text value
    n: number // The field-length norm
  }

  // 'author.tags.name': [{
  //   'v': 'pizza lover',
  //   'i': 2,
  //   'n: 0.7071067811865475
  // }
  type RecordEntryArrayItem = ReadonlyArray<RecordEntryObject & { i: number }>

  // TODO: this makes it difficult to infer the type. Need to think more about this
  type RecordEntry = { [key: string]: RecordEntryObject | RecordEntryArrayItem }

  // {
  //   i: 0,
  //   '$': {
  //     '0': { v: "Old Man's War", n: 0.5773502691896258 },
  //     '1': { v: 'Codenar', n: 1 },
  //     '2': [
  //       { v: 'pizza lover', i: 2, n: 0.7071067811865475 },
  //       { v: 'helo wold', i: 1, n: 0.7071067811865475 },
  //       { v: 'hello world', i: 0, n: 0.7071067811865475 }
  //     ]
  //   }
  // }
  type FuseIndexObjectRecord = {
    i: number // The index of the record in the source list
    $: RecordEntry
  }

  // {
  //   keys: null,
  //   list: [
  //     { v: 'one', i: 0, n: 1 },
  //     { v: 'two', i: 1, n: 1 },
  //     { v: 'three', i: 2, n: 1 }
  //   ]
  // }
  type FuseIndexStringRecord = {
    i: number // The index of the record in the source list
    v: string // The text value
    n: number // The field-length norm
  }

  type FuseIndexRecords =
    | ReadonlyArray<FuseIndexObjectRecord>
    | ReadonlyArray<FuseIndexStringRecord>

  // {
  //   name: 'title',
  //   weight: 0.7
  // }
  export type FuseOptionKeyObject = {
    name: string | string[]
    weight: number
  }

  export type FuseOptionKey = FuseOptionKeyObject | string | string[]

  export interface IFuseOptions<T> {
    isCaseSensitive?: boolean
    distance?: number
    findAllMatches?: boolean
    getFn?: FuseGetFunction<T>
    ignoreLocation?: boolean
    ignoreFieldNorm?: boolean
    includeMatches?: boolean
    includeScore?: boolean
    keys?: Array<FuseOptionKey>
    location?: number
    minMatchCharLength?: number
    shouldSort?: boolean
    sortFn?: FuseSortFunction
    threshold?: number
    useExtendedSearch?: boolean
  }

  // Denotes the start/end indices of a match
  //                 start    end
  //                   ↓       ↓
  type RangeTuple = [number, number]

  export type FuseResultMatch = {
    indices: ReadonlyArray<RangeTuple>
    key?: string
    refIndex?: number
    value?: string
  }

  export type FuseSearchOptions = {
    limit: number
  }

  export type FuseResult<T> = {
    item: T
    refIndex: number
    score?: number
    matches?: ReadonlyArray<FuseResultMatch>
  }

  export type Expression =
    | { [key: string]: string }
    | {
        $path: ReadonlyArray<string>
        $val: string
      }
    | { $and?: Expression[] }
    | { $or?: Expression[] }
}
