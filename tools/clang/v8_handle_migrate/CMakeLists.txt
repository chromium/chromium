set(LLVM_LINK_COMPONENTS
  Support
  )

add_llvm_executable(v8_handle_migrate
  HandleMigrate.cpp
  )

target_link_libraries(v8_handle_migrate
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
)

cr_install(TARGETS v8_handle_migrate RUNTIME DESTINATION bin)
