# UI contract inventory

All APIs are single-owner-thread, non-reentrant, and return `cog_result_t`.
No runtime owns terminal descriptors. The application routes Cog stdout/stderr
before construction and restores descriptors after cleanup. Only constructors
allocate runtime ownership; settings buffers are caller-owned fixed arrays.

| Header | Types | Functions | Critical invariants |
|---|---|---|---|
| settings.h | cog_tui_settings_t | defaults, validate, load, save | No raw credentials; bounded arrays; load output unchanged on error; old file preserved before rename. |
| runtime.h | opaque runtime, options, status | new, submit, step, snapshot, cancel, reload, shutdown, free | No private Cog calls; one manual driver; snapshot borrows expire on mutation; shutdown drains before free. |

Runtime wrapper functions carry the `cog_tui_runtime_` prefix and settings
functions carry `cog_tui_settings_`. Exact argument/error contracts are in headers.
Provider choice is OpenRouter in this release; mock is explicitly a test mode.
Tools and persistent Cog sessions are disabled. The current conversation remains
in Cog's app and its public display snapshot, avoiding a duplicate transcript.

Readiness means the local runtime accepted configuration, not a successful future
network request. Reload success means published for a future turn, not activated.
Visible outcome uses public message status, never undocumented numeric CLI state.
A submit return of OK means queued; local UI busy state is cleared only after a
new terminal assistant display item or explicit runtime failure, not on submit.

Settings format: five lines, `version=1`, `provider=...`, `model=...`,
`base_url=...`, `credential_ref=...`. Writer emits that order. Reader accepts any
order exactly once, LF terminated (final LF optional), rejects blank/extra lines,
unknown keys, duplicates, embedded NUL/CR, oversized content, incomplete selection.
Credential references match `env:[A-Za-z_][A-Za-z0-9_]*`. Values have no ASCII
controls. HTTPS URL must have a nonempty host and no userinfo, query or fragment.
No credential value is loaded until the Cog constructor/reload resolver needs it.

Tests must cover every row of the scenario document plus strict standalone
headers. Atomic save errors before replacement retain the previous file. A
post-rename directory durability error is reported as indeterminate, because the
new file may already be visible. No operation expands shell syntax.
