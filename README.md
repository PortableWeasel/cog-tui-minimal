# cog-tui-minimal

A lightweight C11/readline terminal UI for Cog. Run `cog-tui`, configure an
OpenRouter model and credential reference once, then return directly to a prompt.
The UI has its own repository, build, settings, and tests. All Cog integration
uses installed public application/core APIs; Cog source is unchanged.

## Build and run

Requires CMake, a C11 compiler, readline development files, and a compatible
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

Use an ANSI/VT-compatible terminal. Assistant chunks join into a continuous
message above a separator; the readline prompt stays on the bottom row while
output scrolls. Long input scrolls horizontally. Draft text and its cursor are
preserved during streaming and terminal resize.

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
chunks exercise editing, cancellation and continuation; they do not call a real
provider. Mock model names can be changed through `/settings`.

## Update the installed commands

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

The [streaming Markdown blueprint](docs/markdown/BLUEPRINT.md) plans reversible
partial-syntax previews and a backend-neutral renderer suitable for ncursesw.
It includes draft headers, scenario traces, an architecture diagram and staged
implementation gates. This is planned work; the installed renderer is plain text.
