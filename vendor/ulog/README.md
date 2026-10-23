# ulog dependency

Vendored from [ThirdLetterC/ulog](https://github.com/ThirdLetterC/ulog) at
revision `99cf8b6e515eee0353eddd024b354551fd99f2dc`.

Only the public header, single-translation-unit implementation, and upstream
MIT license are included. The build enables dynamic topics and one custom
output for ezgrpc2's per-server log routing.

The implementation includes a small `__auto_type` compatibility alias so the
upstream C23 source also compiles with GCC/Clang C2x fallback modes.
