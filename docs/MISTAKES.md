# Mistakes — what failed, why, and the checkable rule that prevents it

Format per project-lifecycle: what failed + root cause (not the symptom) + the cost +
a **checkable** prevention rule ("assert X before Y", never "be careful").

---

## 2026-09-20 — `pkill -f <pattern>` kills the tool call that runs it (swallowed command tails)

**What failed:** three compound cleanup commands (`pkill -f cellc.sh; rm -rf …; echo …`)
ended with their tails never executing — no output, no error, stale state left behind
(`/tmp/bench-q30` surviving an "rm -rf", a tmux relaunch silently not happening). The
misleading part: `pkill` itself always "worked" (the target processes did die), so each
command *looked* successful up to the point it vanished.

**Root cause:** `pkill -f` matches the pattern against **every** process's full command
line — including the bash process running the compound command itself, whose cmdline
contains the literal pattern string (`pkill -f cellc.sh; …`). pkill kills the caller;
everything after the pkill in the same command never runs.

**Cost:** ~30 min of confused re-polling across the Qwen3-30B c-suite session; one false
"relaunched" belief; stale bench state briefly misread as "something recreated the dir."

**Prevention rule (checkable):** when killing by pattern inside a compound command, use
a bracketed pattern that cannot match its own command line — `pkill -f "[c]ellc.sh"` —
or split the kill into its own single command and assert afterwards
(`pgrep -f "[c]ellc.sh" || echo dead; ls <path-i-expect-gone> 2>&1`). Never chain
`pkill -f X` with any follow-up statement in the same shell command.
