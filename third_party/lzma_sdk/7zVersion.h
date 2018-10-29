#define MY_VER_MAJOR 15
#define MY_VER_MINOR 14
#define MY_VER_BUILD 0
#define MY_VERSION_NUMBERS "15.14"
#define MY_VERSION "15.14"
#define MY_DATE "2015-12-31"
#undef MY_COPYRIGHT
#undef MY_VERSION_COPYRIGHT_DATE
#define MY_AUTHOR_NAME "Igor Pavlov"
#define MY_COPYRIGHT_PD "Igor Pavlov : Public domain"
#define MY_COPYRIGHT_CR "Copyright (c) 1999-2015 Igor Pavlov"

#ifdef USE_COPYRIGHT_CR
  #define MY_COPYRIGHT MY_COPYRIGHT_CR
#else
  #define MY_COPYRIGHT MY_COPYRIGHT_PD
#endif

#define MY_VERSION_COPYRIGHT_DATE MY_VERSION " : " MY_COPYRIGHT " : " MY_DATE
