set(LLVM_LINK_COMPONENTS
  BitReader
  MCParser
  Option
  X86AsmParser
  X86CodeGen
  )

add_llvm_executable(rewrite_templated_container_fields
  RewriteTemplatedPtrFields.cpp
  ../raw_ptr_plugin/Util.cpp
  ../raw_ptr_plugin/RawPtrHelpers.cpp
  )

target_link_libraries(rewrite_templated_container_fields
  clangAST
  clangASTMatchers
  clangAnalysis
  clangBasic
  clangDriver
  clangEdit
  clangFrontend
  clangLex
  clangParse
  clangSema
  clangSerialization
  clangTooling
  )

cr_install(TARGETS rewrite_templated_container_fields RUNTIME DESTINATION bin)
target_include_directories(rewrite_templated_container_fields PUBLIC "../raw_ptr_plugin")
