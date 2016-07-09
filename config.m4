dnl $Id$
dnl config.m4 for extension phtrace

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(phtrace, for phtrace support,
dnl Make sure that the comment is aligned:
dnl [  --with-phtrace             Include phtrace support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(phtrace, whether to enable phtrace support,
Make sure that the comment is aligned:
[  --enable-phtrace           Enable phtrace support])

if test "$PHP_PHTRACE" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-phtrace -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/phtrace.h"  # you most likely want to change this
  dnl if test -r $PHP_PHTRACE/$SEARCH_FOR; then # path given as parameter
  dnl   PHTRACE_DIR=$PHP_PHTRACE
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for phtrace files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       PHTRACE_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$PHTRACE_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the phtrace distribution])
  dnl fi

  dnl # --with-phtrace -> add include path
  dnl PHP_ADD_INCLUDE($PHTRACE_DIR/include)

  dnl # --with-phtrace -> check for lib and symbol presence
  dnl LIBNAME=phtrace # you may want to change this
  dnl LIBSYMBOL=phtrace # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $PHTRACE_DIR/$PHP_LIBDIR, PHTRACE_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_PHTRACELIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong phtrace lib version or lib not found])
  dnl ],[
  dnl   -L$PHTRACE_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(PHTRACE_SHARED_LIBADD)

  PHP_NEW_EXTENSION(phtrace, phtrace.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
