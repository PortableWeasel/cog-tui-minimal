# Stage 5 — Integration and terminal research

## Verified route

Use the installed public single-process application interface in `cog_cli/cli.h`
with the cooperative driver in `cog/core.h`. A standalone probe in
`pocs/public-app` compiled using only installed package exports and observed a
mock assistant snapshot before graceful shutdown and teardown. Cog's documented
external-consumer fixture is precedent for public app embedding. No private
source/header is linked or copied.

The intended adapter calls `cog_cli_app_new`, `cog_cli_app_submit`,
`cog_core_drive_begin`, `cog_core_step`, `cog_cli_app_display_snapshot`,
`cog_cli_app_cancel`, `cog_cli_app_reload`, `cog_cli_app_shutdown`,
`cog_core_drive_end`, and `cog_cli_app_free`. Exact wrapper contracts and errors
will be reviewed in the header stages. App-owned asynchronous work must drain
before destruction; ending the core driver alone does not free that work.

The headless host/client C implementation is private and not installed. Do not
consume those private headers or copy their protocol implementation. Embedding
avoids implementing a second lease/replay protocol for this minimum UI.

## Scope decisions

- Support the existing REAL OpenRouter backend. A provider selection currently
  has one real choice; custom model identifiers are entered explicitly. Do not
  imply arbitrary providers are supported by changing an endpoint label.
- Persist an environment credential reference, not raw key material. Setup
  explains the required environment variable and validates reference syntax.
  Runtime construction resolves the reference through Cog. No credential bytes
  enter input history or settings files.
- Use plain text rendering with readline. Ncurses adds no required capability
  for the first UI. Terminal callback editing and redraw still require a PTY
  regression test.
- Use an in-process Cog app. Save a UI output descriptor before routing Cog's
  direct stdout away from the terminal; render authoritative snapshots through
  the saved stream. Route Cog diagnostics separately and surface operation
  error codes without exposing secrets. Descriptor ownership/restoration must
  be explicit and tested.
- Disable tools initially. Existing Cog supports tool execution, but establishing
  useful prompt policy and permission interaction is outside this first minimum
  delivery. Do not present mock text as proof of real tool capability.
- Use one current in-memory session. Persist UI settings only, and disable Cog's
  durable store unless future scope explicitly adds conversation persistence.

## Evidence and remaining work

Inspected installed public `cli.h`, `core.h`, `net.h`, readline's installed
`readline.h`, and Cog's `docs/README.md` external-consumer section. The existing
prefix supplies Cog and its audited networking dependency graph. REAL operation
requires that compatible dependency profile; arbitrary system curl is not an
acceptable substitute under Cog's published net contract.

The probe passed strict compilation and execution. Remaining proofs: callback
readline redraw/input while streaming, multiple turns/cancellation/shutdown,
configuration save failure, reload activation, and REAL construction without
making a paid/live request. Actual live provider acceptance is separate from
mock and local validation and will be reported honestly.

### Readline proof

`pocs/readline/probe.c` passed a PTY check with a partially edited prompt,
simulated output, and continued typing. Readline 8.2 preserved `draft text`.
The callback has no user-context parameter; one explicit callback binding is
allowed solely in the terminal adapter, with single-instance admission and
clearing on removal. Substantive application state remains caller-owned.
