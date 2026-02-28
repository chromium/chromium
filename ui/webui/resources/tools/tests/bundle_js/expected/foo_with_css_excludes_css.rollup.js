import sheet from './foo.css' with { type: 'css' };

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

alert('Hello from resources/foo_resource.js');

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

alert('Hello from external/bar/bar.js');

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

alert('Hello from external/foo/foo.js');

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

alert('Hello from external/baz/baz.js');

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


alert('Hello from src/foo.js');

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


document.getElementById('foo').shadowRoot.adoptedStyleSheets = [sheet];
//# sourceMappingURL=foo_with_css.rollup.js.map
