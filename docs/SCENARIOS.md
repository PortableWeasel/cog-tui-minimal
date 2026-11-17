# Scenario traces — contract pass 2

Each name below omits the `cog_tui_` prefix. Settings calls are declared in
`include/cog_tui/settings.h`; runtime calls in `include/cog_tui/runtime.h`.
The diagrams show responsibility flow; implementation tests remain pending.

## 01. First run

```text
User/UI → defaults → input edits → validate → save → runtime_new → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | settings_defaults, settings_validate, settings_save; runtime_new |

Gap analysis: Missing model must lead to setup; no provider traffic.

## 02. Configured relaunch

```text
User/UI → load → runtime_new → snapshot → prompt → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | settings_load; runtime_new, runtime_snapshot |

Gap analysis: Private settings restore configured prompt readiness.

## 03. Prompt and incremental output

```text
User/UI → submit → step → snapshot → render → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_submit, runtime_step, runtime_snapshot |

Gap analysis: Queued acceptance differs from terminal completion; never use numeric state IDs.

## 04. Second turn

```text
User/UI → submit → step → terminal snapshot → submit → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_submit, runtime_step, runtime_snapshot |

Gap analysis: Same runtime preserves actual Cog conversation.

## 05. Edit while streaming

```text
User/UI → readline edits ⇄ step → snapshot → redraw → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_step, runtime_snapshot |

Gap analysis: Readline integration is application-owned; draft/cursor survive redraw.

## 06. Cancel

```text
User/UI → cancel → step → snapshot → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_cancel, runtime_step, runtime_snapshot |

Gap analysis: Cancellation request is not a terminal result; keep driving.

## 07. Normal exit

```text
User/UI → shutdown → step until done → free → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_shutdown, runtime_step, runtime_free |

Gap analysis: No free before done; no tool execution in this profile.

## 08. Settings change

```text
User/UI → validate → save → reload → step → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | settings_validate, settings_save; runtime_reload, runtime_step |

Gap analysis: Saved selection and published future selection labeled separately.

## 09. Save failure

```text
User/UI → save → error → retain edits → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | settings_save |

Gap analysis: Old file intact before rename; after-rename durability needs explicit result.

## 10. Invalid settings file

```text
User/UI → load → error → setup → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | settings_load, settings_defaults |

Gap analysis: Output unchanged; reject duplicate keys and unsafe permissions.

## 11. Missing credential

```text
User/UI → load → runtime_new → failure → correct reference → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | settings_load; runtime_new |

Gap analysis: No secret in diagnostics; runtime constructor cleans partial ownership.

## 12. Provider failure

```text
User/UI → step → snapshot/error → visible outcome → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_step, runtime_snapshot |

Gap analysis: No automatic resubmit; incomplete text remains visible.

## 13. Resize

```text
User/UI → terminal resize → snapshot → redraw → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_snapshot |

Gap analysis: Application preserves prompt; no Cog state mutation for redraw.

## 14. Reload failure

```text
User/UI → reload → error → preserve runtime → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_reload |

Gap analysis: Failed publication does not claim activation; saved preferences may differ.

## 15. Mock proof

```text
User/UI → runtime_new(mock) → submit → step → snapshot → shutdown → free → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | runtime_new, runtime_submit, runtime_step, runtime_snapshot, runtime_shutdown, runtime_free |

Gap analysis: Offline integration is not proof of real provider access.

## 16. Limits and teardown failure

```text
User/UI → submit/load → limit or step error → shutdown/free → UI
```

| Header coverage | Calls |
|---|---|
| settings.h / runtime.h as named above | settings_load; runtime_submit, runtime_step, runtime_shutdown, runtime_free |

Gap analysis: Report exhaustion; retain runtime when unsafe to free; no silent truncate.

## Pass 2 findings (resolved by pass 3 contracts)

- Atomic save needs a post-rename durability outcome distinct from ordinary IO.
- Credential grammar and file line rules need exact specification.
- Runtime has no documented numeric idle ID; use public display status plus
  committed nonzero assistant node identity. Active INCOMPLETE aggregates are
  not terminal.
- Reload publication is not activation; retain conservative selection labels.
- Free after a fatal core driver result requires a supported drain route; tools
  remain disabled, and failure cleanup must be checked in implementation tests.
