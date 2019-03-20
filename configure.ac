AC_INIT(src/cairoDevice.c)

AC_CHECK_PROGS(PKG_CONFIG, pkg-config)   
if test -n "${PKG_CONFIG}"; then
	if (${PKG_CONFIG} --exists gtk+-2.0); then
         GTK_CPPFLAGS=`${PKG_CONFIG} --cflags gtk+-2.0`
         GTK_LIBS=`${PKG_CONFIG} --libs gtk+-2.0`
    else
         echo "ERROR: gtk+2.0 not found by pkg-config."
         exit 1
    fi
else
    echo "ERROR: Cannot find pkg-config."
    exit 1
fi

## Allow user to set extra preprocessor flags (e.g. -Wall)
GTK_CPPFLAGS=${PKG_CPPFLAGS}" "${GTK_CPPFLAGS}

AC_SUBST(GTK_CPPFLAGS)
AC_SUBST(GTK_LIBS)
AC_OUTPUT(src/Makevars)

