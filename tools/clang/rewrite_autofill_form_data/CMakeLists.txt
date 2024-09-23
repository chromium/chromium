set(LLVM_LINK_COMPONENTS
  BitReader
  MCParser
  Option
  X86AsmParser
  X86CodeGen
  )

add_llvm_executable(rewrite_autofill_form_data
  FieldToFunction.cpp
  )

target_link_libraries(rewrite_autofill_form_data
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

cr_install(TARGETS rewrite_autofill_form_data RUNTIME DESTINATION bin)
