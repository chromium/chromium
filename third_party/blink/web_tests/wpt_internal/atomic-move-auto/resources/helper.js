// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function reparent(parent, child) {
    const action = new URL(location.href).searchParams.get("action");
    switch (action) {
        case "append":
            parent.append(child);
            break;
        case "prepend":
            parent.prepend(child);
            break;
        case "before": {
            const fake = document.createElement("div");
            parent.appendChild(fake);
            fake.before(child);
            parent.removeChild(fake);
            break;
        }
        case "after": {
            const fake = document.createElement("div");
            parent.appendChild(fake);
            fake.after(child);
            parent.removeChild(fake);
            break;
        }
        default:
            throw new Error(`Unknown action: ${action}`);
    }
}