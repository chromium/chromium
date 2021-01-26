(function() {

// This function stabilize the line number in console messages from this script.
function log(str) {
  console.log(str);
}

class MockFileChooserFactory extends EventTarget {
  constructor() {
    super();
    this.paths_ = [];
    this.baseDir_ = undefined;
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.FileChooser);
    this.interceptor_ =
        new MojoInterfaceInterceptor(blink.mojom.FileChooser.name);
    this.interceptor_.oninterfacerequest = e => {
      this.bindingSet_.addBinding(
          new MockFileChooser(this, this.paths_, this.baseDir_), e.handle);
      this.paths_ = [];
    };
    this.interceptor_.start();
  }

  setPathsToBeChosen(paths, opt_baseDir) {
    // TODO(tkent): Need to ensure each of paths is an absolute path.
    this.paths_ = paths;
    this.baseDir_ = opt_baseDir;
  }
}

function modeToString(mode) {
  let Mode = blink.mojom.FileChooserParams.Mode;
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
    log(`FileChooser: opened; mode=${modeToString(params.mode)}`);

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
    if (this.paths_.length > 0) {
      log('FileChooser: selected: ' + this.paths_);
    } else {
      log('FileChooser: canceled');
    }
    const file_info_list = [];
    for (const path of this.paths_) {
      file_info_list.push(new blink.mojom.FileChooserFileInfo({
          nativeFile: {
              filePath: toFilePath(path),
              displayName: {data:[]}
          }
      }));
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

window.mockFileChooserFactory = new MockFileChooserFactory();

})();
