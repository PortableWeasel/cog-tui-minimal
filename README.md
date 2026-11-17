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

Run `cog-tui --mock` for an explicit offline terminal fixture. Its delayed mock
chunks exercise editing, cancellation and continuation; they do not call a real
provider. Mock model names can be changed through `/settings`.

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
