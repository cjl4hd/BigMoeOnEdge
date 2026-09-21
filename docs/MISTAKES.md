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

---

## 2026-09-21 — `setsid nohup … &` engines survive their tmux session's teardown ("DOWN" while UP)

**What failed:** session-15's wrap-up recorded the LFM2.5 daily driver as DOWN after
killing the serve tmux session — but the engine had been launched `setsid nohup … &`,
so it detached from the session and survived. The stale engine (started Sun 15:04)
squatted RAM through the night and into session-19's RAM-constrained quality gate;
the resume asserted a false environment state that the process table contradicted.
Found only by accident, reading a `ps` output for an unrelated question.

**Root cause:** `setsid nohup cmd &` re-parents the process to init — the tmux
session is no longer its ancestor, so `tmux kill-session` cannot reach it. The
belt-and-suspenders launch pattern (session wrapper + setsid detach) creates a
process no teardown step in the workflow owns or tracks.

**Cost:** ~8 h of unknown contention on a 4-core host; ~2 GB anon + page cache
missing during a gate that needs it; one false "DOWN" resume entry trusted until
session 19.

**Prevention rule (checkable):** any engine started detached gets its PID recorded
in PROGRESS before wrap-up (`pgrep -f "[b]moe-serve.py"`), and every "server
UP/DOWN" claim in the resume is backed by a process assertion, not by teardown
intent: `pgrep -f "[b]moe-serve.py" && echo UP || echo DOWN`. Never claim teardown
without the assert — `tmux kill-session` does not kill setsid-reparented children;
kill the recorded PID (`kill <pid>`), then assert the process is gone.
