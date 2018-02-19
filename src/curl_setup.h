#pragma once

#define HAVE_STRDUP

typedef char bool;

#define ISALPHA isalpha
#define ISDIGIT isdigit
#define ISBLANK isblank
#define ISALNUM isalnum
#define ISSPACE isspace

#define argv_item_t  char *
#  define DIR_CHAR      "\\"
#  define DOT_CHAR      "_"

#define FOPEN_READTEXT "rt"
#define FOPEN_WRITETEXT "wt"
#define FOPEN_APPENDTEXT "at"
#  define O_RDONLY 0x0000

#define WHILE_FALSE  while(0)

#    define ssize_t __int64
#define true 1
#define false 0

#  include <io.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  undef  lseek
#  define lseek(fdes,offset,whence)  _lseeki64(fdes, offset, whence)
#  undef  fstat
#  define fstat(fdes,stp)            _fstati64(fdes, stp)
#  undef  stat
#  define stat(fname,stp)            _stati64(fname, stp)
#  define struct_stat                struct _stati64
#  define LSEEK_ERROR                (__int64)-1

#  define Curl_nop_stmt do { } WHILE_FALSE

#define DEBUGASSERT(x) do { } WHILE_FALSE
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
