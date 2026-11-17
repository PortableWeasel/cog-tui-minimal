# Readline streaming draft probe

Compile with `cc -std=c11 -Wall -Wextra -Werror pocs/readline/probe.c -lreadline`.
A PTY driver typed `draft`, waited for a simulated output line, then typed
` text\n`. The accepted input remained `draft text`; the output contained the
streamed line. This establishes basic draft preservation with installed readline
8.2, not production resize/signal/EOF behavior.

The callback lacks a context argument. Production uses one explicitly bound
callback context with single-instance admission and teardown clearing; all
substantive state remains caller-owned. This isolated process-global bridge is
a documented exception for readline's own singleton API.
