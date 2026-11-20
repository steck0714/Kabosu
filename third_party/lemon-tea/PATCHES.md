# Changes against upstream

Upstream: https://github.com/citrus610/lemon-tea-2, commit edc3915b67863ed4c2fb10ad3fa218ac0a46889d

Copied: `bot/*` and `lib/avec.h`, `lib/hash.h`, `LICENSE`.

Not copied: `client/`, `tbp/`, `ppt/`, `tuner/`, `lib/pge`, `lib/nlohmann`, `lib/ppt_sync`.

1. Line endings converted from CRLF to LF.
2. `bot/def.h`: `#include <x86intrin.h>` is now only included on x86-64, so the
   bot compiles for aarch64. `_pext_u64` is only used when `USE_PEXT` is defined;
   without it `board.cpp` uses a portable fallback.

Known things to revisit (not changed yet):
- `bot/def.h` and `bot/search.h` pull in `<iostream>` (debug printing); remove for a small sysmodule.
- The search thread is a `std::thread` created in `bot/thread.cpp`. On x86 it needs a
  stack of roughly 128 KB (64 KB crashed). Check the default thread stack size on libnx.
- `Thread::advance()` is asynchronous: wait for the bot thread to process it before the
  next `Thread::request()`, otherwise `request()` returns a stale plan.
- Piece order differs from the game: bot is I,J,L,O,S,T,Z; the game is S,Z,J,L,T,O,I (0..6).
