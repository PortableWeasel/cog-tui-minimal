# CPack distribution

CPack creates a binary TGZ and a source TGZ, each with a SHA-256 sidecar.
The binary contains `cog-tui`, the optional Python 3 Pi launcher, this guide,
the README, and the MD4C license. It consumes Cog as a separately installed
public SDK; it does not contain or compile Cog sources.

## Build packages

Build Cog's Release distribution first. Use a fresh build directory when changing
Cog installations. Set `COG_PREFIX` below to its extracted
prefix containing `include`, `lib`, and `bin`. Match Cog's `CMAKE_INSTALL_LIBDIR`
(default `lib` for the distribution profile) in the UI build.

```sh
cmake -S . -B build-dist -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$COG_PREFIX" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build-dist -j4
ctest --test-dir build-dist --output-on-failure
(cd build-dist && cpack --config CPackConfig.cmake)
(cd build-dist && cpack --config CPackSourceConfig.cmake)
```

Set `CPACK_GENERATOR` / `CPACK_SOURCE_GENERATOR` at configure time or pass
`cpack -G ZIP` to select another available generator. CPack's archive generation
uses the normal CMake install rules. The source archive stages only source,
headers, tests, scripts, build configuration and documentation; it excludes
checkout metadata, build/install trees and local runtime settings. Maintain the
explicit `cmake/source-files.txt` manifest when adding release source files.

## Extract and run

Extract the separate Cog and TUI archives into the **same prefix**, dropping
each archive's top directory. Do this as your own user, in an empty directory:

```sh
mkdir -p "$PWD/cog-release"
tar -xzf /path/to/cog-0.1.0-Linux-x86_64.tar.gz \
  --strip-components=1 -C "$PWD/cog-release"
tar -xzf /path/to/cog-tui-minimal-0.1.0-Linux-x86_64.tar.gz \
  --strip-components=1 -C "$PWD/cog-release"
./cog-release/bin/cog-tui --help
./cog-release/bin/cog-tui --mock
```

Linux installs default to a relative `$ORIGIN/../lib` runtime search path,
so the combined prefix can be moved. Explicit caller RPATH settings are
honored. The binary needs a compatible Linux architecture/glibc, readline and
its terminal library, a UTF-8 ANSI terminal, and Cog's audited shared networking
dependencies. Cog's vendored distribution supplies those networking libraries;
the TUI archive alone does not. This is not a universal Linux binary.

For the Pi launcher, install Python 3 and configure Pi's OpenRouter provider.
`bin/cog-tui-pi` finds the adjacent `cog-tui` even when the prefix is absent from
PATH. `COG_TUI_BIN` still overrides that selection. `cog-update` stays a
source-checkout helper and is not installed into the binary package.

## Release limitations

These archives are technical release candidates. The repository has no project
license file establishing redistribution terms for its original code; obtain
an explicit licensing decision before public redistribution. MD4C's MIT terms
are included and do not establish terms for this project's own code. Cog and
its bundled dependencies have their own notices and release limitations.

Packaging does not change the existing live-provider acceptance limitations in
`VALIDATION.md`. Offline tests and mock startup do not establish paid provider
readiness. No personal settings, credentials or conversation history belong in
a distribution.
