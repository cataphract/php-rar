dnl $Id$
dnl config.m4 for extension rar

PHP_ARG_ENABLE(rar, whether to enable rar support,
[  --enable-rar            Enable rar support])

unrar_sources="unrar/sha256.cpp unrar/qopen.cpp \
               unrar/blake2s.cpp unrar/recvol.cpp \
               unrar/headers.cpp unrar/match.cpp \
               unrar/find.cpp \
               unrar/resource.cpp \
               unrar/pathfn.cpp \
               unrar/dll.cpp unrar/threadpool.cpp unrar/volume.cpp \
               unrar/unpack.cpp \
               unrar/extract.cpp unrar/errhnd.cpp \
               unrar/crc.cpp unrar/rijndael.cpp unrar/crypt.cpp \
               unrar/rawread.cpp \
               unrar/rs.cpp unrar/smallfn.cpp \
               unrar/isnt.cpp unrar/rar.cpp unrar/consio.cpp \
               unrar/scantree.cpp unrar/archive.cpp unrar/strfn.cpp \
               unrar/strlist.cpp \
               unrar/getbits.cpp unrar/hash.cpp \
               unrar/filestr.cpp \
               unrar/extinfo.cpp unrar/ui.cpp unrar/rarvm.cpp \
               unrar/timefn.cpp unrar/sha1.cpp \
               unrar/rdwrfn.cpp unrar/rs16.cpp unrar/cmddata.cpp \
               unrar/extractchunk.cpp unrar/system.cpp \
               unrar/unicode.cpp unrar/filcreat.cpp \
               unrar/arcread.cpp unrar/filefn.cpp \
               unrar/global.cpp unrar/list.cpp \
               unrar/encname.cpp unrar/file.cpp \
               unrar/secpassword.cpp unrar/options.cpp"


AC_LANG_PUSH([C++])
cxxflags_null=""
AC_DEFUN([CXXC_FLAG_CHECK],
[
ac_saved_cxxflags="$CXXFLAGS"
CXXFLAGS="$1 -Werror"
flag_to_add=m4_default([$2],[$1])
AC_MSG_CHECKING([whether the C++ compiler supports $1])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([])],
  [AC_MSG_RESULT([yes])]
  [cxxflags_null="$cxxflags_null $flag_to_add"],
  [AC_MSG_RESULT([no])]
)
CXXFLAGS="$ac_saved_cxxflags"
])
CXXC_FLAG_CHECK([-Wparentheses], [-Wno-parentheses])
CXXC_FLAG_CHECK([-Wswitch], [-Wno-switch])
CXXC_FLAG_CHECK([-Wdangling-else], [-Wno-dangling-else])
CXXC_FLAG_CHECK([-Wunused-function], [-Wno-unused-function])
CXXC_FLAG_CHECK([-Wunused-variable], [-Wno-unused-variable])
CXXC_FLAG_CHECK([-Wsign-compare], [-Wno-sign-compare])
CXXC_FLAG_CHECK([-Wmisleading-indentation], [-Wno-misleading-indentation])
AC_LANG_POP([C++])

extra_cxxflags="$cxxflags_null"
echo "EXTRA_CXXFLAGS := \$(EXTRA_CXXFLAGS) $extra_cxxflags" >> Makefile.fragments
cat Makefile.frag >> Makefile.fragments
INCLUDES=`echo "$INCLUDES" | sed 's/-I/-isystem /g'`

if test "$PHP_RAR" != "no"; then
  AC_DEFINE(HAVE_RAR, 1, [Whether you have rar support])
  PHP_SUBST(RAR_SHARED_LIBADD)
  PHP_REQUIRE_CXX()
  PHP_ADD_LIBRARY_WITH_PATH(stdc++, "", RAR_SHARED_LIBADD)

  PHP_NEW_EXTENSION(rar, rar.c rar_error.c rararch.c rarentry.c rar_stream.c rar_navigation.c rar_time.c $unrar_sources, $ext_shared,,-DRARDLL -DSILENT -Wno-write-strings -Wall -fvisibility=hidden -I@ext_srcdir@/unrar)
  PHP_ADD_BUILD_DIR($ext_builddir/unrar)
fi
