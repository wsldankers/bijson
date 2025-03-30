AC_DEFUN([MY_CHECK_BUILTIN], [
	AS_VAR_PUSHDEF([ac_var], [ax_cv_have_builtin_$1])

	AC_CACHE_CHECK([for __builtin_$1], [ac_var], [
		AC_LINK_IFELSE([AC_LANG_PROGRAM([], [__builtin_$1($2)])],
			[AS_VAR_SET([ac_var], [yes])], [AS_VAR_SET([ac_var], [no])])
	])

	case AS_VAR_GET([ac_var]) in yes)
		AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_BUILTIN_$1), 1, [has __builtin_$1 function])
	esac

	AS_VAR_POPDEF([ac_var])
])
