# MD4C 0.5.2

Unmodified upstream C parser and public header from tag `release-0.5.2`:
<https://github.com/mity/md4c/tree/release-0.5.2>.
Retrieved 2026-11-17 from `raw.githubusercontent.com/mity/md4c/release-0.5.2/`.
`md4c.c` and `md4c.h` are from `src/`; `LICENSE.md` is from the repository root.
License: MIT; retained in `LICENSE.md` and upstream source headers.

Pinned SHA-256:

```
d30937367d5413e7eaa218b1640b8946ff76fd34d97152f6979fd96169d5d0fc  LICENSE.md
52e39843b96b64d0281bf795128bcc88c9c4dc52d7436737e9f4306648dc4590  md4c.c
4efd19bf7ec270691d5b4189f496886e421768a814b5e817eb945aa85e859f18  md4c.h
```

Built privately as `tui-md4c`. Upstream warnings are suppressed for the unchanged
vendor target; all UI code retains strict warnings. Sanitizer builds instrument
this target as well as the adapter. No network download occurs at build time,
and neither dependency nor configuration is shared with Cog.

Update policy: explicitly replace this pin, preserve the license, record new
hashes, and rerun parser prefix/provenance, allocation-failure, fuzz, terminal,
and sanitizer gates before adopting a new upstream version.
