plan() {
	printf '1..%d\n' "$1"
}

check() {
	msg=$1
	shift
	if eval "$@" 2>/dev/null 1>&2; then
		printf 'ok - %s\n' "$msg"
	else
		printf 'not ok - %s\n' "$msg"
		false
	fi
	true
}

check_same() {
	msg=$1
	shift
	eval "$1" 2>/dev/null 1>&2 >out1 || true
	eval "$2" 2>/dev/null 1>&2 >out2 || true
	diff -u out1 out2 || true
	check "$msg" cmp out1 out2
}
