// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import cpp

/* Mojo C++ Interface */
class MojoCppInterface extends Class {
  MojoCppInterface() {
    this.getQualifiedName().matches("%::mojom::%") and
    this.getAMember().hasName("Proxy_")
  }
}

/* Mojo Interface Implementation*/
class MojoInterfaceImpl extends Class {
  MojoInterfaceImpl() {
    this.getABaseClass+() instanceof MojoCppInterface and
    not this.isAbstract() and
    not this.getQualifiedName().matches("%::mojom::%")
  }

  MemberFunction getAMethod() {
    result = this.getAMemberFunction() and
    result.overrides(this.getABaseClass+().getAMemberFunction()) and
    not result instanceof Constructor and
    not result instanceof Destructor
  }
}
