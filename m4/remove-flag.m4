AC_DEFUN([MY_REMOVE_FLAG], [dnl
	AC_PREREQ(2.64)
	AS_VAR_PUSHDEF([FLAGS],[m4_default($2,_AC_LANG_PREFIX[FLAGS])])
	spaced=" AS_VAR_GET(FLAGS) "
	case $spaced in (*@<:@$IFS@:>@"$1"@<:@$IFS@:>@*)
		before=${spaced%%"$1"@<:@$IFS@:>@*}
		after=${spaced#*@<:@$IFS@:>@"$1"}
		spaced="$before $after"
		spaced=${spaced%@<:@$IFS@:>@}
		spaced=${spaced#@<:@$IFS@:>@}
		AS_VAR_SET(FLAGS,["$spaced"])
		AC_RUN_LOG([: FLAGS="$FLAGS"])
	;; (*)
		AC_RUN_LOG([: FLAGS did not contain $1])
	esac
	AS_VAR_POPDEF([FLAGS])dnl
])dnl MY_REMOVE_FLAG
