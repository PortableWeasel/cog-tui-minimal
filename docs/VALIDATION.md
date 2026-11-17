# Validation status

## Completed evidence

- Three header/scenario passes plus independent contract review before implementation.
- Installed public Cog app embedding probe and readline PTY draft-preservation probe.
- GCC strict C11/Werror build; standalone compilation of every UI public header.
- Settings suite: defaults, URL/reference validation, bounded parser and malformed
  files, permissions/symlinks, atomic save/load and restrictive umask.
- Runtime suite: empty initialization run, multi-turn snapshots, double-submit
  exclusion, reload publication, cancellation, graceful shutdown and destructor
  lifecycle.
- PTY suite: initial mock readiness, streaming, cancellation after partial output,
  fresh continuation with a shorter response, third turn, settings change/save,
  resize while editing, terminal mode restoration, exit and relaunch restoring
  the saved model.
- Normal install and installed `--help` smoke.

GCC and Clang ASan/UBSan passed all three final suites with the compatible
external dependency profile, including REAL construction without submission. Sanitizers instrument this UI; the existing Cog
installation is not rebuilt with sanitizers by those flags.

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

## Not yet claimed

No paid/live provider request has been sent. Real multi-turn network acceptance
and system-prompt/tool behavior are unverified. Tool execution is disabled.
The sixteen scenario traces are design coverage; they are not sixteen separately
executed scenario tests. Forced I/O failure, arbitrary provider
failure timing, and every shutdown race are not yet exhaustively tested.

## Reproducing the local compatible profile

This workspace's successful construction tests used unchanged Cog source and
its existing audited curl build. From the UI checkout:

```sh
cog_source=/home/user/ws-2611/cog
cmake -S "$cog_source" -B build-cog -DCOG_USE_VENDORED_LIBS=OFF \
  -DBUILD_TESTING=OFF -DCOG_ENABLE_WERROR=ON \
  -DCOG_NET_CURL_BUILD_DIR="$cog_source/build-deps/curl" \
  -DCOG_NET_CURL_LIBRARY="$cog_source/build-deps/curl/lib/libcurl.so" \
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
