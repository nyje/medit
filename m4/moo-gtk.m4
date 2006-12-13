##############################################################################
# _MOO_CHECK_VERSION(pkg-name)
#
AC_DEFUN([_MOO_CHECK_VERSION],[
if test x$MOO_OS_CYGWIN != xyes; then
  PKG_CHECK_MODULES($1,$2)

  AC_MSG_CHECKING($1 version)
  _moo_ac_version=`$PKG_CONFIG --modversion $2`

  $1[]_VERSION=$_moo_ac_version
  $1[]_MAJOR_VERSION=`echo "$_moo_ac_version" | $SED 's/\([[^.]][[^.]]*\).*/\1/'`
  $1[]_MINOR_VERSION=`echo "$_moo_ac_version" | $SED 's/[[^.]][[^.]]*.\([[^.]][[^.]]*\).*/\1/'`
  $1[]_MICRO_VERSION=`echo "$_moo_ac_version" | $SED 's/[[^.]][[^.]]*.[[^.]][[^.]]*.\(.*\)/\1/'`

  m4_foreach([num],[2,4,6,8,10,12,14],
  [AM_CONDITIONAL($1[]_2_[]num, test $[]$1[]_MINOR_VERSION -ge num)])

  AC_MSG_RESULT($[]$1[]_MAJOR_VERSION.$[]$1[]_MINOR_VERSION.$[]$1[]_MICRO_VERSION)
fi
])


##############################################################################
# _MOO_CHECK_BROKEN_GTK_THEME
#
AC_DEFUN([_MOO_CHECK_BROKEN_GTK_THEME],[
AC_ARG_WITH([broken-gtk-theme], AC_HELP_STRING([--with-broken-gtk-theme], [Work around bug in gtk theme]), [
  if test x$with_broken_gtk_theme = "xyes"; then
    MOO_BROKEN_GTK_THEME="yes"
  fi
])

if test x$MOO_BROKEN_GTK_THEME = xyes; then
  AC_MSG_NOTICE([Broken gtk theme])
  AC_DEFINE(MOO_BROKEN_GTK_THEME, 1, [broken gtk theme])
fi
])


##############################################################################
# MOO_PKG_CHECK_GTK_VERSIONS
#
AC_DEFUN([MOO_PKG_CHECK_GTK_VERSIONS],[
AC_REQUIRE([MOO_AC_CHECK_OS])
_MOO_CHECK_VERSION(GTK, gtk+-2.0)
_MOO_CHECK_VERSION(GLIB, glib-2.0)
_MOO_CHECK_VERSION(GTHREAD, gthread-2.0)
_MOO_CHECK_VERSION(GDK, gdk-2.0)
dnl _MOO_CHECK_BROKEN_GTK_THEME
])
