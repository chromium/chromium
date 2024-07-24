set(LLVM_LINK_COMPONENTS
  X86AsmParser
  X86CodeGen
  )

add_llvm_executable(spanify
  Spanifier.cpp
  ../raw_ptr_plugin/Util.cpp
  ../raw_ptr_plugin/RawPtrHelpers.cpp
  )

target_link_libraries(spanify
  clangASTMatchers
  clangTooling
  )

cr_install(TARGETS spanify RUNTIME DESTINATION bin)
target_include_directories(spanify PUBLIC "../raw_ptr_plugin")
