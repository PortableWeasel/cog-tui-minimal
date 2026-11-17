# Transcript contract passes before implementation

M5 introduces transcript.h/c to inject synthetic public snapshots without Cog
changes. No new provider behavior or private data access is involved.

1. Initial header: owner-thread sync identities, compose scene, scroll, free.
   Separate sync from paint so terminal outcomes continue every tick. Scene
   lifetime ends at next mutation; borrowed snapshots never enter cache.
2. Scenario pass: equal-length replacement, shorter terminal commit, phase-only
   commit, replay generation change and resized paused view require exact cache
   bytes plus key/phase/window, stable promotion by preceding user identity,
   and a public identity inspection for deterministic tests. Unknown session or
   duplicate durable identity fails explicitly. Uncached entries count dirty.
3. Final gap pass: memory limits include metadata and actual frame capacities
   via markdown frame_storage. Visible uncached frames have a separate aggregate
   bound. Cache eviction must not free frames already borrowed into current
   scene. On allocation/validation failure app can cleanly close instead of
   presenting a falsely current old frame. Page bindings preserve readline draft;
   anchor uses stable message key and logical row, not array position.

The renderer is borrowed; transcript never paints or mutates Cog. Input and
notice ring remain app-owned. These passes extend INTEGRATION_PLAN.md's three
passes before transcript.c implementation.

Final failure-path amendment before implementation: add transcript_fail() to mark
a stopped runtime's still-active authoritative prefix FAILED without altering
source or fabricating a durable ID. A cancel request alone never calls it.
Rechecked sticky lifetime, cache phase invalidation, and exact final render.

## Implemented integration and evidence

The app uses a sink for readline output, a custom redisplay callback that copies
input, and one screen painter. Bell policy and the three changed keymap entries
are restored. Ctrl-P pages backward, Ctrl-N forward, Ctrl-T returns to the live
tail without changing the draft. Direct entry replacement preserves previous
macro/function/keymap ownership; `rl_copy_keymap` is unsuitable here because its
nested maps can share ownership. Completion menus and readline's auxiliary
search UI are suppressed; ordinary editing/paste/accept stay in readline.

Snapshot synchronization runs every service tick. Rendering is coalesced to
34 ms for output changes; input, resize, notices and terminal phases repaint
immediately. Finalized frame caching uses exact bytes, phase, width and requested
window with stable UI keys. Same-session replay generations preserve keys.
Changed source of equal length, shorter final text, and unchanged bytes with a
terminal phase all invalidate cached rendering. A fatal runtime error marks a
remaining active prefix FAILED through transcript_fail(), without source edits.

The cache budget includes the transcript object and exact renderer allocations
plus copied source, up to 4 MiB excluding allocator bookkeeping. Uncached visible
frames have a separate 4 MiB aggregate cap. Oversized source is rendered directly
from the live snapshot through PLAIN without retaining a copy. Limits or parsing
failures produce a visible plain-view status. OOM/error uses clean teardown.

Notices use eight bounded app-owned buffers; long notices are split at 511 bytes.
Recent notice frames get available output rows, leaving half for conversation
when present. Notices are cleared only on a successfully submitted new prompt.
The screen's separator holds paused/plain status and the input remains separate.
Tiny terminals bypass invalid renderer geometry and use the screen resize notice.

Executed normal CTest: 19/19 passed, excluding local TLS provider-failure because
that test needs sandbox socket escalation. Executed Clang ASan/UBSan with
`detect_leaks=0`: 19/19 passed under the same exclusion. Root must run full tests
with leak detection and the local socket fixture outside sandbox restrictions.
Synthetic transcript coverage includes key promotion, equal-length/shrinking
source, phase changes, cancelled/fatal results, replay identity, scroll anchor
through resize, plain table overflow, and >4 MiB uncached source tail retention.
PTY/libvterm coverage includes draft/cursor editing, asynchronous output, resize,
scroll pause/tail bindings, Ctrl-L hidden output suppression, setup/save/reload,
EOF/signals/output hangup and terminal restoration.

An observed public mock limitation is recorded rather than hidden: after a
cancelled scripted turn, Cog may commit an empty COMPLETE assistant result on
the next input. The UI renders that exact empty result. The old PTY test no
longer assumes nonexistent continuation payload; it checks admission and a
subsequent fresh prompt instead. No Cog changes or fabricated snapshot text.

## Critic corrections

The terminal owner now explicitly saves/enables/restores bracketed-paste mode
(DEC private mode 2004), because readline's prep/deprep writes go to the sink.
App saves the configured readline enable-bracketed-paste value, enables parsing
for this session even if inputrc disables it, and restores the value on teardown.
The PTY test starts with that inputrc setting off, verifies mode bytes, and pastes
a multiline draft without implicit submission. Global durable IDs are validated
across all snapshot roles; duplicate user/assistant IDs return PROTOCOL. Focused
normal integration tests pass 5/5 after both corrections.

Final paste teardown correction: libvterm ignores private-mode save/restore, so
close now sends explicit mode-2004 disable before attempted restore. Supporting
terminals preserve prior-on state; unsupported terminals safely return disabled.
The cell fixture now calls libvterm's paste API after open and close, proving
actual mode state instead of only matching escape bytes.
