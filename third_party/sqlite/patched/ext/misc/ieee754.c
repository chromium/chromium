/*
** 2013-04-17
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This SQLite extension implements functions for the exact display
** and input of IEEE754 Binary64 floating-point numbers.
**
**   ieee754(X)
**   ieee754(Y,Z)
**
** In the first form, the value X should be a floating-point number.
** The function will return a string of the form 'ieee754(Y,Z)' where
** Y and Z are integers such that X==Y*pow(2,Z).
**
** In the second form, Y and Z are integers which are the mantissa and
** base-2 exponent of a new floating point number.  The function returns
** a floating-point value equal to Y*pow(2,Z).
**
** Examples:
**
**     ieee754(2.0)       ->     'ieee754(2,0)'
**     ieee754(45.25)     ->     'ieee754(181,-2)'
**     ieee754(2, 0)      ->     2.0
**     ieee754(181, -2)   ->     45.25
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>

/*
** Implementation of the ieee754() function
*/
static void ieee754func(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  if( argc==1 ){
    sqlite3_int64 m, a;
    double r;
    int e;
    int isNeg;
    char zResult[100];
    assert( sizeof(m)==sizeof(r) );
    if( sqlite3_value_type(argv[0])!=SQLITE_FLOAT ) return;
    r = sqlite3_value_double(argv[0]);
    if( r<0.0 ){
      isNeg = 1;
      r = -r;
    }else{
      isNeg = 0;
    }
    memcpy(&a,&r,sizeof(a));
    if( a==0 ){
      e = 0;
      m = 0;
    }else{
      e = a>>52;
      m = a & ((((sqlite3_int64)1)<<52)-1);
      m |= ((sqlite3_int64)1)<<52;
      while( e<1075 && m>0 && (m&1)==0 ){
        m >>= 1;
        e++;
      }
      if( isNeg ) m = -m;
    }
    sqlite3_snprintf(sizeof(zResult), zResult, "ieee754(%lld,%d)",
                     m, e-1075);
    sqlite3_result_text(context, zResult, -1, SQLITE_TRANSIENT);
  }else if( argc==2 ){
    sqlite3_int64 m, e, a;
    double r;
    int isNeg = 0;
    m = sqlite3_value_int64(argv[0]);
    e = sqlite3_value_int64(argv[1]);
    if( m<0 ){
      isNeg = 1;
      m = -m;
      if( m<0 ) return;
    }else if( m==0 && e>1000 && e<1000 ){
      sqlite3_result_double(context, 0.0);
      return;
    }
    while( (m>>32)&0xffe00000 ){
      m >>= 1;
      e++;
    }
    while( m!=0 && ((m>>32)&0xfff00000)==0 ){
      m <<= 1;
      e--;
    }
    e += 1075;
    if( e<0 ) e = m = 0;
    if( e>0x7ff ) e = 0x7ff;
    a = m & ((((sqlite3_int64)1)<<52)-1);
    a |= e<<52;
    if( isNeg ) a |= ((sqlite3_uint64)1)<<63;
    memcpy(&r, &a, sizeof(r));
    sqlite3_result_double(context, r);
  }
}


#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_ieee_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErrMsg;  /* Unused parameter */
  rc = sqlite3_create_function(db, "ieee754", 1, SQLITE_UTF8, 0,
                               ieee754func, 0, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function(db, "ieee754", 2, SQLITE_UTF8, 0,
                                 ieee754func, 0, 0);
  }
  return rc;
}
