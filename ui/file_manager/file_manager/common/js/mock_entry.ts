// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileErrorToDomError} from './util.js';

/** Joins paths so that the two paths are connected by only 1 '/'. */
export function joinPath(a: string, b: string): string {
  return a.replace(/\/+$/, '') + '/' + b.replace(/^\/+/, '');
}

/** Mock class for DOMFileSystem. */
export class MockFileSystem implements FileSystem {
  entries: Record<string, Entry> = {};
  readonly rootURL: string;

  /**
   * @param name Volume ID.
   * @param rootURL URL string of root which is used in MockEntry.toURL.
   */
  constructor(public name: string, rootURL?: string) {
    this.entries['/'] = MockDirectoryEntry.create(this, '/');
    this.rootURL = rootURL || 'filesystem:' + name + '/';
  }

  get root(): DirectoryEntry {
    return this.entries['/']! as DirectoryEntry;
  }

  /**
   * Creates file and directory entries for all the given entries.  Entries can
   * be either string paths or objects containing properties 'fullPath',
   * 'metadata', 'content'.  Paths ending in slashes are interpreted as
   * directories.  All intermediate directories leading up to the
   * files/directories to be created, are also created.
   * @param entries An array of either string file paths, objects containing
   *     'fullPath' and 'metadata', or Entry to populate in this file system.
   * @param clear If true clears all entries before populating.
   */
  populate(entries: Array<string|Entry>, clear: boolean = false) {
    if (clear) {
      this.entries = {'/': MockDirectoryEntry.create(this, '/')};
    }

    entries.forEach(entry => {
      if (entry instanceof MockEntry) {
        this.entries[entry.fullPath] = entry;
        entry.filesystem = this;
        return;
      }

      let path: string;
      let metadata: Metadata|undefined;
      let content: Blob|undefined;

      if (typeof (entry) === 'string') {
        path = entry;
      } else {
        path = entry.fullPath;
        metadata = (entry as MockEntry).metadata;
        content = (entry as MockFileEntry).content;
      }

      const pathElements = path.split('/');
      pathElements.forEach((_, i) => {
        const subpath = pathElements.slice(0, i).join('/');
        if (subpath && !(subpath in this.entries)) {
          this.entries[subpath] =
              MockDirectoryEntry.create(this, subpath, metadata);
        }
      });

      // If the path doesn't end in a slash, create a file.
      if (!/\/$/.test(path)) {
        this.entries[path] =
            MockFileEntry.create(this, path, metadata, content);
      }
    });
  }

  /**
   * Returns all children of the supplied directoryEntry.
   * @param directory parent directory to find children of.
   */
  findChildren(directory: MockDirectoryEntry): Entry[] {
    const parentPath = directory.fullPath.replace(/\/?$/, '/');
    const children: Entry[] = [];
    for (const path in this.entries) {
      if (path.indexOf(parentPath) === 0 && path !== parentPath) {
        const nextSeparator = path.indexOf('/', parentPath.length);
        // Add immediate children files and directories...
        if (nextSeparator === -1 || nextSeparator === path.length - 1) {
          children.push(this.entries[path]!);
        }
      }
    }

    return children;
  }
}

export interface MockEntryInterface {
  /**
   * Clones the entry with the new full path.
   *
   * @param fullPath New full path.
   * @param filesystem New file system
   * @return Cloned entry.
   */
  clone(fullPath: string, filesystem?: FileSystem): Entry;
}

/** Base class of mock entries. */
export class MockEntry implements Entry, MockEntryInterface {
  removed = false;
  isFile = true;
  isDirectory = false;

  constructor(
      public filesystem: MockFileSystem, public fullPath: string,
      public metadata: Metadata = {} as Metadata) {
    this.filesystem.entries[this.fullPath] = this;
    this.metadata.size ??= 0;
    this.metadata.modificationTime ??= new Date();
  }

  /** Name of the entry. */
  get name(): string {
    return this.fullPath.replace(/^.*\//, '');
  }

  /** Gets metadata of the entry. */
  getMetadata(
      onSuccess: (md: Metadata) => void, onError?: (e: FileError) => void) {
    if (this.filesystem.entries[this.fullPath]) {
      onSuccess?.(this.metadata);
    } else {
      onError?.({name: FileErrorToDomError.NOT_FOUND_ERR} as FileError);
    }
  }

  /** Returns fake URL. */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  toURL(): string {
    const segments = this.fullPath.split('/');
    for (let i = 0; i < segments.length; i++) {
      segments[i] = encodeURIComponent(segments[i]!);
    }

    return joinPath(this.filesystem.rootURL, segments.join('/'));
  }

  /** Gets parent directory. */
  getParent(
      onSuccess?: (a: DirectoryEntry) => void, onError?: (e: Error) => void) {
    const path = this.fullPath.replace(/\/[^\/]+$/, '') || '/';
    const entry = this.filesystem.entries[path];
    if (entry) {
      onSuccess?.(entry as DirectoryEntry);
    } else {
      onError?.({name: FileErrorToDomError.NOT_FOUND_ERR} as Error);
    }
  }

  /**
   * Moves the entry to the directory.
   *
   * @param parent Destination directory.
   * @param newName New name.
   * @param onSuccess Callback invoked with the moved entry.
   */
  moveTo(
      parent: DirectoryEntry, newName?: string, onSuccess?: (a: Entry) => void,
      _onError?: (e: FileError) => void) {
    delete this.filesystem.entries[this.fullPath];
    const newPath = joinPath(parent.fullPath, newName || this.name);
    const newFs = parent.filesystem;
    // For directories, also move all descendant entries.
    if (this.isDirectory) {
      for (const e of Object.values(this.filesystem.entries)) {
        if (e.fullPath.startsWith(this.fullPath)) {
          delete this.filesystem.entries[e.fullPath];
          (e as MockEntry)
              .clone(e.fullPath.replace(this.fullPath, newPath), newFs);
        }
      }
    }

    onSuccess?.(this.clone(newPath, newFs));
  }

  copyTo(
      parent: DirectoryEntry, newName?: string, onSuccess?: (a: Entry) => void,
      _onError?: (e: FileError) => void) {
    const entry = this.clone(
        joinPath(parent.fullPath, newName || this.name), parent.filesystem);
    onSuccess?.(entry);
  }

  /** Removes the entry. */
  remove(onSuccess: VoidCallback, _onError?: (e: FileError) => void) {
    this.removed = true;
    delete this.filesystem.entries[this.fullPath];
    onSuccess?.();
  }

  /** Removes the entry and any children. */
  removeRecursively(
      onSuccess: VoidCallback, _onError?: (e: FileError) => void) {
    this.removed = true;

    for (const path in this.filesystem.entries) {
      if (path.startsWith(this.fullPath)) {
        (this.filesystem.entries[path] as MockEntry).removed = true;
        delete this.filesystem.entries[path];
      }
    }

    onSuccess?.();
  }

  /** Asserts that the entry was removed. */
  assertRemoved() {
    if (!this.removed) {
      throw new Error('expected removed for file ' + this.name);
    }
  }

  clone(_fullPath: string, _fileSystem?: FileSystem): Entry {
    throw new Error('Not implemented');
  }
}

/** Mock class for FileEntry. */
export class MockFileEntry extends MockEntry implements MockEntryInterface {
  static create(
      filesystem: MockFileSystem, fullPath: string, metadata?: Metadata,
      content?: Blob): FileEntry {
    return new MockFileEntry(filesystem, fullPath, metadata, content);
  }

  override readonly isFile = true;
  override readonly isDirectory = false;

  /** Use create() instead, so the instance gets the |FileEntry| type. */
  private constructor(
      filesystem: MockFileSystem, fullPath: string, metadata?: Metadata,
      public content: Blob = new Blob([])) {
    super(filesystem, fullPath, metadata);
  }

  /** Gets a File that this object represents. */
  file(onSuccess: (f: File) => void, _onError?: (e: FileError) => void) {
    onSuccess?.(new File([this.content], this.toURL()));
  }

  /** Gets a FileWriter. */
  createWriter(
      onSuccess: (w: FileWriter) => void, _onError?: (e: FileError) => void) {
    onSuccess?.(new MockFileWriter(this));
  }

  override clone(path: string, filesystem?: MockFileSystem): FileEntry {
    return MockFileEntry.create(
        filesystem || this.filesystem, path, this.metadata, this.content);
  }

  /** Helper to expose methods mixed in via MockEntry to the type checker. */
  asMock(): MockEntry {
    return this;
  }

  asFileEntry(): FileEntry {
    return this;
  }
}

/** Mock class for FileWriter. */
export class MockFileWriter implements FileWriter {
  position: number = 0;
  length: number = 0;
  // eslint-disable-next-line @typescript-eslint/naming-convention
  INIT: number = 0;
  // eslint-disable-next-line @typescript-eslint/naming-convention
  WRITING: number = 0;
  // eslint-disable-next-line @typescript-eslint/naming-convention
  DONE: number = 0;
  readyState: number = 0;
  error: Error = new Error('Not implemented');

  onwriteend = (_e: ProgressEvent<EventTarget>) => {};
  onwritestart = (_e: ProgressEvent<EventTarget>) => {};
  onprogress = (_e: ProgressEvent<EventTarget>) => {};
  onwrite = (_e: ProgressEvent<EventTarget>) => {};
  onabort = (_e: ProgressEvent<EventTarget>) => {};
  onerror = (_e: ProgressEvent<EventTarget>) => {};

  constructor(private entry_: MockFileEntry) {}

  write(data: Blob) {
    this.entry_.content = data;
    this.onwriteend(new ProgressEvent(
        'writeend',
        {lengthComputable: true, loaded: data.size, total: data.size}));
  }

  abort(): void {
    throw new Error('Not implemented');
  }

  addEventListener(
      _type: string, _callback: EventListenerOrEventListenerObject|null,
      _options?: boolean|AddEventListenerOptions): void {
    throw new Error('Not implemented');
  }

  dispatchEvent(_event: Event): boolean {
    throw new Error('Not implemented');
  }

  removeEventListener(
      _type: string, _callback: EventListenerOrEventListenerObject|null,
      _options?: boolean|EventListenerOptions): void {
    throw new Error('Not implemented');
  }

  seek(_offset: number): void {
    throw new Error('Not implemented');
  }

  truncate(_size: number): void {
    throw new Error('Not implemented');
  }
}

/** Mock class for DirectoryEntry. */
export class MockDirectoryEntry extends MockEntry implements
    MockEntryInterface {
  static create(
      filesystem: MockFileSystem, fullPath: string,
      metadata?: Metadata): DirectoryEntry {
    return new MockDirectoryEntry(filesystem, fullPath, metadata);
  }

  override readonly isFile = false;
  override readonly isDirectory = true;

  /** Use create() instead, so the instance gets the DirectoryEntry type. */
  private constructor(
      filesystem: MockFileSystem, fullPath: string, metadata?: Metadata) {
    super(filesystem, fullPath, metadata);
  }

  override clone(path: string, filesystem?: MockFileSystem) {
    return MockDirectoryEntry.create(filesystem || this.filesystem, path);
  }

  /** Returns all children of the supplied directoryEntry. */
  getAllChildren(): Entry[] {
    return this.filesystem.findChildren(this);
  }

  /** Returns a file under the directory. */
  private getEntry_(
      expectedClass: typeof MockFileEntry|typeof MockDirectoryEntry,
      path: string, option: FileSystemFlags = {},
      onSuccess?: (a: Entry) => void, onError?: (e: FileError) => void) {
    if (this.removed) {
      onError?.({name: FileErrorToDomError.NOT_FOUND_ERR} as FileError);
      return;
    }

    const fullPath = path[0] === '/' ? path : joinPath(this.fullPath, path);
    const result = this.filesystem.entries[fullPath];
    if (result) {
      if (!(result instanceof expectedClass)) {
        onError?.({name: FileErrorToDomError.TYPE_MISMATCH_ERR} as FileError);
      } else if (option['create'] && option['exclusive']) {
        onError?.({name: FileErrorToDomError.PATH_EXISTS_ERR} as FileError);
      } else {
        onSuccess?.(result);
      }
    } else {
      if (!option['create']) {
        onError?.({name: FileErrorToDomError.NOT_FOUND_ERR} as FileError);
      } else {
        const newEntry = expectedClass.create(this.filesystem, fullPath);
        onSuccess?.(newEntry);
      }
    }
  }

  /** Returns a file under the directory. */
  getFile(
      path: string, option?: FileSystemFlags,
      onSuccess?: (a: FileEntry) => void, onError?: (e: FileError) => void) {
    this.getEntry_(
        MockFileEntry, path, option,
        onSuccess as (((e: Entry) => void) | undefined), onError);
  }

  /** Returns a directory under the directory. */
  getDirectory(
      path: string, option?: FileSystemFlags,
      onSuccess?: (a: DirectoryEntry) => void,
      onError?: (e: FileError) => void) {
    this.getEntry_(
        MockDirectoryEntry, path, option,
        onSuccess as (((e: Entry) => void) | undefined), onError);
  }

  /** Creates a MockDirectoryReader for the entry. */
  createReader(): DirectoryReader {
    return new MockDirectoryReader(
        (this.filesystem as MockFileSystem).findChildren(this));
  }
}

/** Mock class for DirectoryReader. */
export class MockDirectoryReader implements DirectoryReader {
  constructor(private readonly entries_: Entry[]) {}

  /**
   * Returns entries from the filesystem associated with this directory in
   * chunks of 2.
   */
  readEntries(
      onSuccess: (a: Entry[]) => void, _onError?: (e: FileError) => void) {
    const chunk = this.entries_.splice(0, 2);
    onSuccess?.(chunk);
  }
}
