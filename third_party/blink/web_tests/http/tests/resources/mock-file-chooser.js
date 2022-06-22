import {FileChooser, FileChooserParams_Mode, FileChooserReceiver} from '/gen/third_party/blink/public/mojom/choosers/file_chooser.mojom.m.js';

class MockFileChooserFactory extends EventTarget {
  constructor() {
    super();
    this.paths_ = [];
    this.baseDir_ = undefined;
    this.receiver_ = undefined;
    this.interceptor_ =
        new MojoInterfaceInterceptor(FileChooser.$interfaceName);
    this.interceptor_.oninterfacerequest = e => {
      this.receiver_ = new FileChooserReceiver(
          new MockFileChooser(this, this.paths_, this.baseDir_));
      this.receiver_.$.bindHandle(e.handle);
      this.paths_ = [];
    };
    this.interceptor_.start();
  }

  setPathsToBeChosen(paths, opt_baseDir) {
    internals.disableReferencedFilePathsVerification();
    // TODO(tkent): Need to ensure each of paths is an absolute path.
    this.paths_ = paths;
    this.baseDir_ = opt_baseDir;
  }
}

function modeToString(mode) {
  let Mode = FileChooserParams_Mode;
  switch (mode) {
  case Mode.kOpen:
    return 'kOpen';
  case Mode.kOpenMultiple:
    return 'kOpenMultiple';
  case Mode.kUploadFolder:
    return 'kUploadFolder';
  case Mode.kSave:
    return 'kSave';
  default:
    return `Unknown ${mode}`;
  }
}

class MockFileChooser {
  constructor(factory, paths, baseDir) {
    this.factory_ = factory;
    this.paths_ = paths;
    this.baseDir_ = baseDir;
  }

  openFileChooser(params) {
    this.params_ = params;

    this.factory_.dispatchEvent(
        new CustomEvent('open', {detail: modeToString(params.mode)}));
    return new Promise((resolve, reject) => {
      setTimeout(() => this.chooseFiles_(resolve), 1);
    });
  }

  enumerateChosenDirectory(directoryPath) {
    console.log('MockFileChooserFactory::EnumerateChosenDirectory() is not implemented.');
  }

  chooseFiles_(resolve) {
    const file_info_list = [];
    for (const path of this.paths_) {
      const nativeFile = {filePath: toFilePath(path), displayName: {data: []}};
      file_info_list.push({nativeFile});
    }
    const basePath = this.baseDir_ || '';
    resolve({result: {files: file_info_list,
                      baseDirectory: toFilePath(basePath)}});
  }
}

function toFilePath(path) {
  if (!navigator.platform.startsWith('Win')) {
    // We assume `path` is absolute, and it is therefore fine as-is on
    // non-Windows systems.
    return {path}
  }

  // On Windows, we rewrite / to \ and return as an array of character codes,
  // since the path's mojom representation on Windows is an array<uint16>
  // instead of a string.
  const winPath = path.replace(/\//g, '\\');
  const string16Path = [];
  for (let i = 0; i < winPath.length; ++i) {
    string16Path.push(winPath.charCodeAt(i));
  }
  return {path: string16Path};
}

export const mockFileChooserFactory = new MockFileChooserFactory();
