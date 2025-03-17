set(LLVM_LINK_COMPONENTS
  BitReader
  MCParser
  Option
  X86AsmParser
  X86CodeGen
  )

add_llvm_executable(ast_rewriter
  ASTRewriter.cpp
  OutputHelper.h
  OutputHelper.cpp
  )

target_link_libraries(ast_rewriter
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

cr_install(TARGETS ast_rewriter RUNTIME DESTINATION bin)
