set(LLVM_LINK_COMPONENTS
  BitReader
  MCParser
  Option
  Support
  X86AsmParser
  X86CodeGen
  X86Desc
  X86Info
  )

add_llvm_executable(string_piece_rewriters
  StringPieceRewriters.cpp
  )

target_link_libraries(string_piece_rewriters
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
  clangToolingCore
  clangTransformer
  )

cr_install(TARGETS string_piece_rewriters RUNTIME DESTINATION bin)
