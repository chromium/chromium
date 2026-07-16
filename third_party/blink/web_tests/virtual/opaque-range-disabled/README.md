This virtual test suite runs with the OpaqueRange flag disabled in order to make
sure Range and StaticRange still inherit from AbstractRange through their hidden
NodeRange parent, which remains unexposed on the global object.
