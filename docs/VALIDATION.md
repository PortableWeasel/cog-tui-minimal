# Validation status

## Markdown release evidence

The current suite has 20 CTest cases: pure Markdown, deterministic prefix fuzz,
allocation failure, screen contracts/cells, transcript reconciliation, settings,
runtime, Pi launcher, terminal integration, and nine failure paths. All UI
production headers compile standalone under strict C11 warnings. The vendored
MD4C source is pinned and instrumented in sanitizer builds; its upstream warnings
are suppressed independently from the strict UI adapter build.

Final full-suite execution passed **20/20 normal GCC tests in 7.46 seconds** and
**20/20 Clang ASan/UBSan tests in 14.39 seconds**, with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. Both runs include the local loopback TLS failure
fixture. The final runs used the required permission for loopback sockets and
LeakSanitizer, superseding earlier restricted-sandbox runs that omitted those
gates. Logs are `build-real/ctest.log` and `build-final-asan/ctest.log`.

Independent critic review approved the renderer and screen/integration after
four findings were corrected. The final UI was installed to
`install/stage-real/bin/cog-tui`, reached through the existing PATH symlink;
the installed streaming/cursor/resize/scrolling/cleanup PTY test passed.
Only the UI was rebuilt and installed during this Markdown release.

Executed coverage includes:

- Markdown headings, emphasis, code, tables, links, lists/quotes; every-prefix
  rendering, provisional EOF boundaries, exact final convergence, Unicode/control
  sanitation, bounded layouts, table-overflow/plain fallback and allocation errors.
- Stable active-to-committed UI identity, equal-length edits, shrinking final text,
  phase-only completion/cancellation, fatal active prefixes, replay generations,
  scroll anchors through reflow, duplicate durable IDs and oversized uncached source.
- Actual libvterm cells: separated prompt, semantic text, Unicode widths, draft and
  byte cursor during asynchronous output/resize, horizontal input clipping,
  Ctrl-P/Ctrl-N paging, Ctrl-T live return, and hidden Ctrl-L output suppression.
- Bracketed-paste enable/explicit-disable/attempted-restore and multiline paste
  retained as a draft, including an inputrc that initially disables it. Actual
  libvterm paste API assertions verify enabled/disabled state. Prior-on mode
  restoration requires terminal private-mode save/restore support.
- Existing setup/settings/reload, credentials-by-reference, file permissions,
  cancellation, EOF/signals, output hangup, save retry and terminal restoration.

`libvterm` is optional and test-only; cell tests return skip code 77 when absent.
Readline completion menus and auxiliary search display are deliberately suppressed.
Ncursesw was not selected: the full hidden-writer/keymap ownership probe remains
incomplete. See [screen findings](markdown/SCREEN_RESEARCH.md) and
[integration review](markdown/TRANSCRIPT_REVIEW.md).

No Cog source or dependency rebuild is needed for this release. UI sanitizer
flags instrument UI and vendored MD4C, not the existing installed Cog libraries.

## Dependency profile finding

The initial existing prefix `cog/install/vendor-relocated` worked in MOCK mode
but REAL construction failed closed with `COG_ERROR_UNSUPPORTED` before any
request. Its `libcog-net.a` expected curl SHA256
`da77f74e16fc6fc483c239d81732bbba242e7784f0980469187af7c668e1ac73`, while the
actually loaded installed curl SHA256 was
`45adbd095d312284888b2e9bd2950d030284d7991243dc73458bb48ed7e59def`.
This is a dependency provenance mismatch, not an authentication failure.
A compatible unchanged-source dependency installation was built in `build-cog`
and installed to `install/cog` inside this worktree. `build-real` and
`build-final-asan` use that prefix and the existing audited curl build artifact.
REAL constructor/snapshot/shutdown passed using a fabricated test credential,
without submitting a prompt or sending a provider request. Installed executable
PTY tests also passed. No admission checks were bypassed.

## Live provider acceptance and limitations

Four paid provider requests were made during this acceptance session; none were
retried automatically. The initial two-request terminal probe observed first
visible output at 0.410 and 0.907 seconds and preserved its input draft. Its raw
ANSI matcher did not find the recall marker, so that probe stopped before its
planned cancellation request. This extraction result was inconclusive.

The subsequent two-request `scripts/cog-live-check --allow-paid` diagnostic
checked authoritative public runtime snapshots and passed with exit status 0:

| Request | Public outcome | First visible prefix | Final bytes | Observed nonempty prefixes |
|---|---|---|---|---|
| Remember marker; acknowledge | Committed COMPLETE; expected acknowledgment found | 1,080 ms | 3 | 1 |
| Recall marker | Committed COMPLETE; exact marker found | 432 ms | 9 | 1 |

The report records `authoritative_marker: true`, `cleanup_complete: true`, no
error and no timeout. Its sanitized numeric/boolean evidence is in
`build/live-acceptance/authoritative.json`; it contains no provider response text
or credential. This establishes two-turn recall through the public runtime API,
not a claim about private provider-history internals. These short responses each
had only one observed nonempty prefix, so they do not establish fine-grained
streaming behavior on their own. Deterministic PTY tests cover incremental UI
rendering, and the separate terminal probe observed live output with a preserved
draft.

**Live cancellation was not exercised.** Cancellation/fresh-input/terminal cleanup
have offline mock and signal-test coverage. Restart readiness using Pi's actual
settings was established by earlier constructor/relaunch checks; the direct
snapshot diagnostic did not add another interactive restart test.

Tool execution remains disabled. The public reload API confirms publication for
a future turn, not activation of that selection. Arbitrary provider failure timing
and every shutdown race are not exhaustively tested. Output-hangup tests check
bounded nonzero exit but cannot capture diagnostics after the terminal is lost.
Cog's deterministic mock can commit an empty post-cancellation response; the UI
preserves that exact public result rather than fabricating continuation text.

## Reproducing the local compatible profile

Rebuilt and installed on 2026-11-17 from Cog commit
`5ce5820003e32d6ae0d15052f881cca05e00b881`, without changing Cog source.
Cog's installed CLI is `install/cog/bin/cog-cli`; the installed UI is
`install/stage-real/bin/cog-tui`. GCC and Clang ASan/UBSan UI suites each passed
the then-current 13 tests, including the Pi launcher. The loopback test requires
socket access outside the restricted sandbox. Installed UI PTY checks passed;
the Pi launcher also reached REAL readiness using the actual Pi settings and
exited cleanly without submitting a prompt. Cog's installed CLI passed its
offline mock smoke check. These installs retain the external audited curl and
c-ares dependency paths; keep `../cog/build-deps` available.

The following historical dependency setup used unchanged Cog source and its
existing audited curl build. It is not rerun for the Markdown/UI-only update.
From the UI checkout:

```sh
cog_source=/home/user/ws-2611/cog
cmake -S "$cog_source" -B build-cog -DCOG_USE_VENDORED_LIBS=OFF \
  -DBUILD_TESTING=OFF -DCOG_ENABLE_WERROR=ON \
  -DCOG_NET_CURL_BUILD_DIR="$cog_source/build-deps/curl" \
  -DCOG_NET_CURL_LIBRARY="$cog_source/build-deps/curl/lib/libcurl.so" \
  -DCMAKE_INSTALL_RPATH="$cog_source/build-deps/curl/lib" \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/cog"
cmake --build build-cog --target cog-cli -j4
cmake --install build-cog
cmake -S . -B build-real -DCMAKE_PREFIX_PATH="$PWD/install/cog" \
  -DCURL_NO_CURL_CMAKE=ON \
  -DCURL_INCLUDE_DIR="$cog_source/build-deps/source/curl-8.23.0/include" \
  -DCURL_LIBRARY_RELEASE="$cog_source/build-deps/curl/lib/libcurl.so" \
  -DCURL_LIBRARY_DEBUG="$cog_source/build-deps/curl/lib/libcurl.so"
cmake --build build-real -j4
ctest --test-dir build-real --output-on-failure
cmake --install build-real --prefix "$PWD/install/stage-real"
```

All generated build/install files stay inside the UI checkout. The audited curl
artifact remains an external dependency; keep its directory available. The
explicit cog-cli target avoids an unrelated upstream all-target fixture link
failure observed with BUILD_TESTING=OFF. This recipe is evidence for this local
profile, not a replacement for Cog's dependency packaging instructions.


## Executable scenario coverage

| Design traces | Executed checks |
|---|---|
| 01 first run | failure-first-run (REAL construction with fabricated reference, no prompt) |
| 02 relaunch, 03 streaming, 04 next turn | terminal and runtime |
| 05 edit during streaming, 06 cancel | terminal, failure-sigint-active |
| 07 exit | terminal, failure-eof, failure-sigint-idle, failure-sigterm-active |
| 08 settings change | terminal and runtime reload |
| 09 save failure | failure-save-retry, settings atomic-write cases |
| 10 invalid file | settings malformed/duplicate/missing/oversized parser cases |
| 11 missing credential, 14 reload refusal | failure-credential-recovery |
| 12 provider failure | failure-provider-failure (loopback TLS peer closes; no external traffic) |
| 13 resize | terminal PTY resize during edited input |
| 15 mock proof | runtime, terminal |
| 16 limits/teardown failure | settings bounded-input tests, failure-output-hangup |

The loopback provider failure produces a terminal failure and clean nonzero
exit with restored terminal modes. No real credential or external endpoint is
used. Credential recovery tests construct/reload only and never submit a prompt.
