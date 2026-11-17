# Implementation strategy and gates

## Dependencies and ownership

| Unit | Requires | Can proceed independently |
|---|---|---|
| Settings | result contract, POSIX filesystem | Runtime adapter |
| Runtime | settings types, installed cog::cli/Curl/net public APIs | Settings file implementation |
| Application | settings + runtime + readline | None until both contracts stable |
| main | app.h | Thin forwarding entry only |

No private Cog headers/source or external daemon is required. Build through the
installed CMake exports. Real profile is OpenRouter only. Tools are disabled
until a separate conditional-tool gate. Include the existing empty config path
explicitly to avoid silently consuming unrelated Cog configuration.

## Contract pass record

1. Pass 1 declared bounded settings and opaque runtime lifecycle.
2. Pass 2 traced sixteen flows; found post-rename durability ambiguity, incomplete
   credential/file grammar, and risks around numeric state/activation assumptions.
3. Pass 3 specified INDETERMINATE save semantics and exact reference grammar,
   retained public snapshot status and conservative future-selection labels,
   and clarified fatal-step teardown in the tool-free profile. Added the app
   entry contract, including readline callback ownership and terminal restoration.
4. Final consistency review: no circular headers, required NULL arguments and
   return codes documented, borrows bounded, constructors paired, mutation occurs
   on owner thread. Standalone C11 strict-warning header compile must pass before
   implementation. Scenario gaps now have named owners; behavioral evidence is
   still required by the tests below.

## Implementation and verification sequence

- Implement settings with contract tests: defaults/incomplete validation, complete
  round trip, invalid/unknown/duplicate/missing keys, oversized/NUL data,
  symlink/permission rejection, prior-file preservation on pre-rename failure.
- Implement runtime and installed mock tests: construct, submit, incremental
  snapshot, multi-turn, cancel, reload, explicit shutdown/drain, stopped free,
  rejected free while running, driver-error cleanup. Never infer tool safety
  from mock success; tools remain disabled.
- Implement app and main with PTY tests: first-run setup, configured relaunch,
  settings change, draft preservation during streaming/resize, Ctrl-C, EOF/quit,
  invalid credentials/configuration recovery and terminal mode restoration.
- Build GCC/Clang with strict warnings and standalone headers. Run CTest, then
  ASan/UBSan in a separate checkout-local build directory. Review source and
  test results independently before integration.
- Install and run against a compatible installed Cog prefix without editing Cog.
  Report real provider testing separately and do not claim it without an actual
  authorized successful request. Offline tests must label mock output clearly.

Use tix implementation/review tickets under milestone 931d35b; dependencies gate
writers and reviewers. Review failure returns to writer; after four failures,
record causes and split/research rather than repeat unchanged work.

## Resource and platform decisions

Initial target is Linux/POSIX terminals. Settings values cap at1023 bytes, provider
at31 bytes, config input at8192 bytes. Runtime limits derive explicitly from Cog
constructor options; do not silently truncate conversation. Readline owns its
process-global dependency state. A temporary single-instance callback binding
is the narrow documented exception to UI global-state avoidance, cleared on exit.
