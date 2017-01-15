dnl $Id$
dnl config.m4 for extension phtrace

PHP_ARG_ENABLE(phtrace, whether to enable phtrace support,
Make sure that the comment is aligned:
[  --enable-phtrace           Enable phtrace support])

if test "$PHP_PHTRACE" != "no"; then
  PHP_NEW_EXTENSION(phtrace, phtrace.c buffer.c, $ext_shared,, -std=c99 -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)

  PHTRACE_OLD_LIBS=$LIBS
  LIBS=
  AC_SEARCH_LIBS([uuid_generate], [uuid], ,[
    AC_MSG_ERROR([unable to find the uuid_generate() function])
  ])
  PHTRACE_SHARED_LIBADD="$PHTRACE_SHARED_LIBADD $LIBS"
  PHP_SUBST(PHTRACE_SHARED_LIBADD)
  LIBS=$PHTRACE_OLD_LIBS;
  unset PHTRACE_OLD_LIBS;
fi
