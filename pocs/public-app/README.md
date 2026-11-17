# Installed public application probe

This standalone throwaway probe verifies a minimal public embedding route.
It includes installed public headers only and links installed CMake targets.
It does not modify or rebuild Cog.

```
cmake -S pocs/public-app -B build-probe -DCMAKE_PREFIX_PATH=/path/to/cog/prefix
cmake --build build-probe
./build-probe/probe
```

Observed against the existing `cog/install/vendor-relocated` installation:
strict GCC compile succeeds; mock submit produces assistant text, the public
snapshot contains that text, graceful shutdown completes, drive ownership ends,
and the app is freed. Exit status 0. Cog itself also writes stdout/stderr, so
production terminal ownership needs explicit stream routing. This proves neither
REAL networking nor all cancellation/error cleanup paths.
