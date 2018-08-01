#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015 6WIND S.A.

# Load config options:
# - DPDK_CHECKPATCH_PATH
# - DPDK_CHECKPATCH_LINE_LENGTH
. $(dirname $(readlink -e $0))/load-devel-config

VALIDATE_NEW_API=$(dirname $(readlink -e $0))/check-symbol-change.sh

length=${DPDK_CHECKPATCH_LINE_LENGTH:-80}

# override default Linux options
options="--no-tree"
options="$options --max-line-length=$length"
options="$options --show-types"
options="$options --ignore=LINUX_VERSION_CODE,\
FILE_PATH_CHANGES,MAINTAINERS_STYLE,SPDX_LICENSE_TAG,\
VOLATILE,PREFER_PACKED,PREFER_ALIGNED,PREFER_PRINTF,\
PREFER_KERNEL_TYPES,BIT_MACRO,CONST_STRUCT,\
SPLIT_STRING,LONG_LINE_STRING,\
LINE_SPACING,PARENTHESIS_ALIGNMENT,NETWORKING_BLOCK_COMMENT_STYLE,\
NEW_TYPEDEFS,COMPARISON_TO_NULL"

clean_tmp_files() {
	if echo $tmpinput | grep -q '^checkpatches\.' ; then
		rm -f "$tmpinput"
	fi
}

trap "clean_tmp_files" SIGINT

print_usage () {
	cat <<- END_OF_HELP
	usage: $(basename $0) [-q] [-v] [-nX|patch1 [patch2] ...]]

	Run Linux kernel checkpatch.pl with DPDK options.
	The environment variable DPDK_CHECKPATCH_PATH must be set.

	The patches to check can be from stdin, files specified on the command line,
	or latest git commits limited with -n option (default limit: origin/master).
	END_OF_HELP
}

check_forbidden_additions() {
    # This awk script receives a list of expressions to monitor
    # and a list of folders to search these expressions in
    # - No search is done inside comments
    # - Both additions and removals of the expressions are checked
    #   A positive balance of additions fails the check
	read -d '' awk_script << 'EOF'
	BEGIN {
		split(FOLDERS,deny_folders," ");
		split(EXPRESSIONS,deny_expr," ");
		in_file=0;
		in_comment=0;
		count=0;
		comment_start="/*"
		comment_end="*/"
	}
	# search for add/remove instances in current file
	# state machine assumes the comments structure is enforced by
	# checkpatches.pl
	(in_file) {
		# comment start
		if (index($0,comment_start) > 0) {
			in_comment = 1
		}
		# non comment code
		if (in_comment == 0) {
			for (i in deny_expr) {
				forbidden_added = "^\+.*" deny_expr[i];
				forbidden_removed="^-.*" deny_expr[i];
				current = expressions[deny_expr[i]]
				if ($0 ~ forbidden_added) {
					count = count + 1;
					expressions[deny_expr[i]] = current + 1
				}
				if ($0 ~ forbidden_removed) {
					count = count - 1;
					expressions[deny_expr[i]] = current - 1
				}
			}
		}
		# comment end
		if (index($0,comment_end) > 0) {
			in_comment = 0
		}
	}
	# switch to next file , check if the balance of add/remove
	# of previous filehad new additions
	($0 ~ "^\+\+\+ b/") {
		in_file = 0;
		if (count > 0) {
			exit;
		}
		for (i in deny_folders) {
			re = "^\+\+\+ b/" deny_folders[i];
			if ($0 ~ deny_folders[i]) {
				in_file = 1
				last_file = $0
			}
		}
	}
	END {
		if (count > 0) {
			print "Warning in " substr(last_file,6) ":"
			print "are you sure you want to add the following:"
			for (key in expressions) {
				if (expressions[key] > 0) {
					print key
				}
			}
			exit RET_ON_FAIL
		}
	}
EOF
	# ---------------------------------
	# refrain from new additions of rte_panic() and rte_exit()
	# multiple folders and expressions are separated by spaces
	awk -v FOLDERS="lib drivers" \
		-v EXPRESSIONS="rte_panic\\\( rte_exit\\\(" \
		-v RET_ON_FAIL=1 \
		"$awk_script" -
}

number=0
quiet=false
verbose=false
while getopts hn:qv ARG ; do
	case $ARG in
		n ) number=$OPTARG ;;
		q ) quiet=true ;;
		v ) verbose=true ;;
		h ) print_usage ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))

if [ ! -f "$DPDK_CHECKPATCH_PATH" ] || [ ! -x "$DPDK_CHECKPATCH_PATH" ] ; then
	print_usage >&2
	echo
	echo 'Cannot execute DPDK_CHECKPATCH_PATH' >&2
	exit 1
fi

total=0
status=0

check () { # <patch> <commit> <title>
	local ret=0

	total=$(($total + 1))
	! $verbose || printf '\n### %s\n\n' "$3"
	if [ -n "$1" ] ; then
		tmpinput=$1
	elif [ -n "$2" ] ; then
		tmpinput=$(mktemp checkpatches.XXXXXX)
		git format-patch --find-renames \
		--no-stat --stdout -1 $commit > "$tmpinput"
	else
		tmpinput=$(mktemp checkpatches.XXXXXX)
		cat > "$tmpinput"
	fi

	report=$($DPDK_CHECKPATCH_PATH $options "$tmpinput" 2>/dev/null)
	if [ $? -ne 0 ] ; then
		$verbose || printf '\n### %s\n\n' "$3"
		printf '%s\n' "$report" | sed -n '1,/^total:.*lines checked$/p'
		ret=1
	fi

	! $verbose || printf '\nChecking API additions/removals:\n'
	report=$($VALIDATE_NEW_API "$tmpinput")
	if [ $? -ne 0 ] ; then
		printf '%s\n' "$report"
		ret=1
	fi

	! $verbose || printf '\nChecking forbidden tokens additions:\n'
	report=$(check_forbidden_additions <"$tmpinput")
	if [ $? -ne 0 ] ; then
		printf '%s\n' "$report"
		ret=1
	fi

	clean_tmp_files
	[ $ret -eq 0 ] && return 0

	status=$(($status + 1))
}

if [ -n "$1" ] ; then
	for patch in "$@" ; do
		# Subject can be on 2 lines
		subject=$(sed '/^Subject: */!d;s///;N;s,\n[[:space:]]\+, ,;s,\n.*,,;q' "$patch")
		check "$patch" '' "$subject"
	done
elif [ ! -t 0 ] ; then # stdin
	subject=$(while read header value ; do
		if [ "$header" = 'Subject:' ] ; then
			IFS= read next
			continuation=$(echo "$next" | sed -n 's,^[[:space:]]\+, ,p')
			echo $value$continuation
			break
		fi
	done)
	check '' '' "$subject"
else
	if [ $number -eq 0 ] ; then
		commits=$(git rev-list --reverse origin/master..)
	else
		commits=$(git rev-list --reverse --max-count=$number HEAD)
	fi
	for commit in $commits ; do
		subject=$(git log --format='%s' -1 $commit)
		check '' $commit "$subject"
	done
fi
pass=$(($total - $status))
$quiet || printf '\n%d/%d valid patch' $pass $total
$quiet || [ $pass -le 1 ] || printf 'es'
$quiet || printf '\n'
exit $status
