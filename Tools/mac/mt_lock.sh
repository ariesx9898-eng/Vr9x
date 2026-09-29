# Shared lock for the Mac tools that change the project's content (build_and_setup.sh, run_editor_python.sh,
# build_hlods.sh, build_pcg.sh). Two of them at once overwrite each other's imports, and play.sh must not start the
# game - or a second setup - while one runs. Source this file, then:
#   mt_lock_acquire "<what is running>" || exit 3
# The lock is a directory (mkdir is atomic) holding the owner's pid; a lock whose owner is gone is taken over.
MT_LOCK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/Saved/mt_content.lock"

# Prints "<what> (pid N)" while a live process holds the lock, nothing otherwise.
mt_lock_holder() {
	local pid
	pid="$(cat "$MT_LOCK_DIR/pid" 2>/dev/null || true)"
	if [ -z "$pid" ] && [ -d "$MT_LOCK_DIR" ]; then
		sleep 1 # just taken: the owner writes its pid right after mkdir
		pid="$(cat "$MT_LOCK_DIR/pid" 2>/dev/null || true)"
	fi
	if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		echo "$(cat "$MT_LOCK_DIR/what" 2>/dev/null || echo "a content build") (pid $pid)"
	fi
}

mt_lock_acquire() {
	# build_and_setup.sh holds it for the whole run, including the scripts it calls.
	if [ "${MT_LOCK_HELD:-0}" = "1" ]; then
		return 0
	fi
	mkdir -p "$(dirname "$MT_LOCK_DIR")"
	if ! mkdir "$MT_LOCK_DIR" 2>/dev/null; then
		local holder
		holder="$(mt_lock_holder)"
		if [ -n "$holder" ]; then
			echo "ERROR: $holder is already changing this project; wait for it to finish." >&2
			return 1
		fi
		rm -rf "$MT_LOCK_DIR" # stale: its owner is gone
		if ! mkdir "$MT_LOCK_DIR" 2>/dev/null; then
			echo "ERROR: could not take the lock $MT_LOCK_DIR" >&2
			return 1
		fi
	fi
	echo "$$" > "$MT_LOCK_DIR/pid"
	echo "${1:-$(basename "$0")}" > "$MT_LOCK_DIR/what"
	export MT_LOCK_HELD=1
	trap 'rm -rf "$MT_LOCK_DIR"' EXIT
	trap 'exit 130' INT TERM HUP
}
