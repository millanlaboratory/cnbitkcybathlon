m4_define([lib_current],5)
m4_define([lib_revision],3)
m4_define([lib_age],2)

AC_INIT(cnbicybathlon,[lib_current.lib_revision.lib_age],
		luca.tonin@epfl.ch)
AC_CONFIG_SRCDIR([src/mi_cybathlon_utilities.hpp])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config/config.h])

AM_INIT_AUTOMAKE([no-dist-gzip dist-bzip2 color-tests subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

AC_PROG_LIBTOOL
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_11

LT_INIT([win32-dll disable-static])
AC_SUBST(LIBTOOL_DEPS)

save_LIBS="$LIBS"

AC_CHECK_HEADERS([lpttrigger.h], [], 
				 AC_MSG_ERROR([The lpttrigger headers must be installed.]), [])
AC_CHECK_HEADERS([SDL/SDL.h], [], 
				 AC_MSG_ERROR([The libSDL headers must be installed.]), [])
AC_CHECK_HEADERS([SDL/SDL_mixer.h], [], 
				 AC_MSG_ERROR([The libSDL_mixer headers must be installed.]), [])

AC_SEARCH_LIBS([OpenLPTTrigger], [lpttrigger], [], 
			   AC_MSG_ERROR([The lpttrigger library must be installed.]))
AC_SEARCH_LIBS([SDL_Init], [SDL], [], 
			   AC_MSG_ERROR([The libSDL library must be installed.]))
AC_SEARCH_LIBS([Mix_OpenAudio], [SDL_mixer], [], 
			   AC_MSG_ERROR([The libSDL_mixer library must be installed.]))
AC_SEARCH_LIBS([dtk_create_window], [drawtk], [], 
			   AC_MSG_ERROR([The drawtk library must be installed.]))
AC_SEARCH_LIBS_CLASS([TCBlock], [tobicore], [#include <tobicore/TCBlock.hpp>], ,
			AC_MSG_ERROR([The tobicore library cannot be found.]))
AC_SEARCH_LIBS_CLASS([ICMessage], [tobiic], [#include <tobiic/ICMessage.hpp>], , 
			AC_MSG_ERROR([The tobiic library cannot be found.]))
AC_SEARCH_LIBS_CLASS([IDMessage], [tobiid], [#include <tobiid/IDMessage.hpp>], ,
			AC_MSG_ERROR([The tobiid library cannot be found.]))
AC_SEARCH_LIBS_CLASS([CcFile], [cnbicore], [#include <cnbicore/CcFile.hpp>], ,
			AC_MSG_ERROR([The cnbicore library cannot be found.]))
AC_SEARCH_LIBS_CLASS([ClClient], [cnbiloop], [#include <cnbiloop/ClClient.hpp>], ,
			AC_MSG_ERROR([The cnbiloop library cannot be found.]))
AC_SEARCH_LIBS_CLASS([CCfgConfig], [cnbiconfig], [#include <cnbiconfig/CCfgConfig.hpp>], ,
			AC_MSG_ERROR([The cnbiconfig library cannot be found.]))

LIBS="$save_LIBS"

AC_CHECK_PROG([DOXYGEN], [doxygen], [doxygen] [no])
if test "$DOXYGEN" = no ; then 
	AC_MSG_ERROR(Cannot find doxygen.) 
fi

AC_HEADER_STDBOOL

#AX_DEFINE_DIR([SYSCONFDIR], [sysconfdir], [System configuration path])

AC_SUBST([RELEASE],[lib_release])
AC_SUBST([CURRENT],[lib_current])
AC_SUBST([REVISION],[lib_revision])
AC_SUBST([AGE],[lib_age])
AC_SUBST(AM_LDFLAGS)

AC_CONFIG_FILES([Makefile src/Makefile extra/Makefile])

AC_OUTPUT
