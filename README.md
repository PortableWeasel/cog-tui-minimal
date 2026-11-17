# cog-tui-minimal

A lightweight C11/readline terminal UI for Cog. Run `cog-tui`, configure an
OpenRouter model and credential reference once, then return directly to a prompt.
The UI has its own repository, build, settings, and tests. All Cog integration
uses installed public application/core APIs; Cog source is unchanged.

## Build and run

Requires CMake, a C11 compiler, readline development files, a UTF-8 locale, and a compatible
installed Cog prefix with its audited networking dependencies.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/installed/cog
cmake --build build -j4
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$PWD/install/stage"
install/stage/bin/cog-tui
```

The installed executable retains the external Cog library directory in its runtime
search path. Keep that dependency prefix available. No Cog private headers,
source-tree includes, or source copying are part of this build.

Set your credential environment variable before launching, for example
`OPENROUTER_API_KEY`, using your normal secret-management workflow. First-run
setup asks for a model identifier, HTTPS endpoint, and an `env:NAME` reference
(default `env:OPENROUTER_API_KEY`). It never asks you to paste a raw key. The
variable must exist in the launched process; exporting a variable in another
shell cannot update a running UI. Select another already-present reference or
restart after changing your environment.

Settings live in the private `~/.cog-tui` directory. Override it with
`--config-dir DIRECTORY`. Files are mode 0600; credential values and prompt
history are not persisted. Only OpenRouter is supported as a real provider in
this version. Model identifiers are entered directly; there is no catalog fetch.

Commands: `/settings`, `/show`, `/cancel`, `/help`, `/quit`. Ctrl-C requests
cancellation while a turn is active and exits while idle. A saved selection and
a published runtime selection are displayed separately; publication is not
reported as proof of activation. Errors require explicit fresh input, with no
automatic resubmission. Tool execution and persistent conversation history are
disabled in this minimum version.

Use an ANSI/VT-compatible terminal with a non-dumb `TERM` and UTF-8 locale.
Assistant messages render Markdown headings, emphasis, lists, quotations,
links, code and tables above a separate readline input row. Open code blocks and
tables have reversible visual boundaries while streaming; the original text
stays unchanged. Unmatched inline syntax remains readable until it resolves.

Ctrl-P and Ctrl-N page the conversation; Ctrl-T resumes following live output.
Scrolling preserves the draft. Long input scrolls horizontally, and draft/cursor
survive streaming and resize. Bracketed multiline pastes remain editable until
Enter; the UI enables this for its session and restores readline settings.
Terminal paste mode is explicitly disabled on exit, then its prior value restored
where private-mode save/restore is supported. Completion menus and readline's auxiliary search display are
suppressed so they cannot overwrite the conversation.

The renderer uses pinned MD4C 0.5.2, privately vendored with its MIT license; no
build-time download is needed. HTML and entities remain literal. Table overflow
or renderer limits switch to an explicitly labeled plain view, preserving the
source. CJK and supported combining clusters use display-cell widths; unsupported
emoji/format characters use visible escapes. Input combining marks are escaped
for deterministic cursor placement. Syntax highlighting, images, interactive
links and speculative inline closure are deferred. Ncursesw is also deferred
pending its full readline ownership probe; the styled-row API is backend-neutral.

To reuse Pi's OpenRouter key and default model, launch from this checkout:

```sh
./scripts/cog-tui-pi
```

The launcher requires Python 3 and reads `~/.pi/agent/settings.json` and
`auth.json` on every launch. Pi's default provider must be `openrouter`.
It exports the credential only to the launched process and refreshes private
settings in `~/.cog-tui-pi`, saving only an environment reference to the key.
Changes made with `/settings` are replaced by Pi's selection on the next launch.
Literal API keys and environment variable names in Pi's auth entry are supported;
command-based credentials (`!command`) are rejected.

It finds `build-real/cog-tui`, `build/cog-tui`, the local staged installs, or
`cog-tui` on PATH. Set `COG_TUI_BIN=/path/to/cog-tui` to choose another executable.
`PI_CODING_AGENT_DIR` overrides Pi's agent directory; `COG_TUI_CONFIG_DIR`
overrides the launcher's settings directory. `--mock` runs the offline fixture
with the imported selection. `--help` prints launcher usage.

Run `cog-tui --mock` for an explicit offline terminal fixture. Its delayed mock
chunks exercise editing and cancellation; they do not call a real
provider. A post-cancellation mock turn can commit an empty response, which the UI
shows exactly as exposed by the public snapshot. Mock model names can be changed through `/settings`.

## Update the installed commands

To update only the UI against the existing installed Cog dependency:

```sh
cmake --build build-real -j4
ctest --test-dir build-real --output-on-failure
cmake --install build-real --prefix "$PWD/install/stage-real"
```

This is the update path used for the Markdown release; it neither rebuilds nor
changes Cog. The existing broader updater below remains available when you
explicitly want both local installations rebuilt.


From this checkout, run:

```sh
./scripts/cog-update
# Also run the Clang sanitizer suite:
./scripts/cog-update --sanitizers
```

After the first run, `cog-update` is available on PATH through `~/.local/bin`,
alongside `cog-cli`, `cog-tui` and `cog-tui-pi`. Run it from any directory.
Restart `cog-tui-pi` after updating; running processes keep their existing code.

The script builds the current sibling `../cog` checkout, installs its CLI and
public libraries to `install/cog`, then builds and tests the TUI before installing
it to `install/stage-real`. It checks the installed commands and creates missing
PATH symlinks, refusing to replace unrelated files. Cog is installed before TUI
validation because the TUI consumes its installed public APIs. A later failure
stops the script; the two installations are not an atomic transaction.
Pi credentials/settings are untouched. It does not pull git changes or download
dependencies.

This script uses the local audited Linux curl profile described in
[validation status](docs/VALIDATION.md). Keep Cog's `build-deps` directory
available. Build output and logs stay in `build-cog`, `build-real`, and optionally
`build-final-asan`. Tests need a terminal-capable environment and permission to
open a local loopback socket. `libvterm` enables the optional screen-emulation
test. Set `COG_SOURCE_DIR`, `COG_CURL_BUILD_DIR`, `COG_CURL_INCLUDE_DIR`,
`COG_UPDATE_JOBS`, or `COG_BIN_DIR` to override the defaults; see
`./scripts/cog-update --help`.

## Verification and design

```sh
cmake -S . -B build-asan -DCMAKE_C_COMPILER=clang -DTUI_SANITIZERS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/installed/cog
cmake --build build-asan -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-asan --output-on-failure
```

See [validation status](docs/VALIDATION.md), [goals](docs/GOALS.md),
[functional blocks](docs/FUNCTIONAL_BLOCKS.md), [cross-check](docs/GOALS_CROSS_CHECK.md),
[diagram](docs/diagram.svg), [research](docs/RESEARCH.md),
[contracts](docs/CONTRACTS.md), [scenarios](docs/SCENARIOS.md), and
[implementation strategy](docs/IMPLEMENTATION_STRATEGY.md).

Development follows `software-blueprint` and `cog-dev`, with tix tracking the
remaining validation and integration work. Live provider acceptance is separate
from offline/mock and construction tests.

The [streaming Markdown blueprint](docs/markdown/BLUEPRINT.md) records the
architecture, scenarios, parser research and implementation decisions. See
[screen ownership evidence](docs/markdown/SCREEN_RESEARCH.md) and
[integration review](docs/markdown/TRANSCRIPT_REVIEW.md) for the shipped boundary.

An optional live diagnostic is deliberately excluded from CTest:

```sh
cmake --build build-real --target live-acceptance
./scripts/cog-live-check --allow-paid
```

It makes at most two short requests using Pi's selected OpenRouter account and
checks multi-turn recall in authoritative public snapshots. It writes only
numeric/boolean results to `build/live-acceptance/authoritative.json`, including
partial results on failure. It never prints or stores credentials or provider
response text and never retries a request. This diagnoses runtime/provider
behavior separately from the deterministic UI/Markdown tests.
