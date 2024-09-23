set(LLVM_LINK_COMPONENTS
  BitReader
  MCParser
  Option
  X86AsmParser
  X86CodeGen
  )

add_llvm_executable(rewrite_raw_ptr_fields
  RewriteRawPtrFields.cpp
  ../raw_ptr_plugin/Util.cpp
  ../raw_ptr_plugin/RawPtrHelpers.cpp
  ../raw_ptr_plugin/StackAllocatedChecker.cpp
  )

target_link_libraries(rewrite_raw_ptr_fields
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

cr_install(TARGETS rewrite_raw_ptr_fields RUNTIME DESTINATION bin)
target_include_directories(rewrite_raw_ptr_fields PUBLIC "../raw_ptr_plugin")
