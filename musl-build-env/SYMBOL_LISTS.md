# glibc compatibility symbol lists

The final image turns these lists into linker-only facade DSOs. Each facade
exports global, absolute `STT_NOTYPE` symbols and carries the corresponding
public glibc SONAME, but has no implementation code or runtime dependencies.
The development `libc.so` linker script considers the facades under
`AS_NEEDED`, so LLD records a dependency exactly when an unresolved symbol
selects it. No post-link dependency rewriting is required.

The `libc.so.6` facade is generated separately from every defined global or
weak dynamic symbol in the pinned musl loader. The five split-library facades
use the authoritative symbol sets below. Symbols also exported by the oldest
supported glibc, CentOS 7's glibc 2.17 `libc.so.6`, are excluded so those
references select the libc facade instead. Their internal filenames start with
`glibc-compat-`, keeping them linker-only: musl resolves each public SONAME to
its built-in libc, while glibc loads the system DSO with that name.

Symbol binding does not affect this filtering: every symbol exported by the
oldest supported libc is excluded from the split-library facades, whether
either DSO exports it weakly or strongly. Explicit `-pthread` and `-lpthread`
links still force the pthread facade into `DT_NEEDED` ahead of libc, and an
explicit `-lm` similarly forces the libm facade ahead of libc.

`glibc-libm-symbols.txt` is the amd64 and arm64 intersection from
`alpine:latest` (Alpine 3.24.1, musl 1.2.6) and `libm.so.6` from Ubuntu 26.04
(glibc 2.43-2ubuntu2), excluding symbols also exported by glibc 2.17 libc. The
two architectures have the same 268 symbols.

glibc 2.34 integrated libpthread, libdl, libutil, and libanl into libc. The
legacy lists use Fedora 34's glibc 2.33-21.fc34 DSOs and the musl 1.2.5-r23
used by this build environment:

- `glibc-2.33-libpthread-symbols-aarch64.txt`: 152 symbols, minus all 46 also
  exported by libc, leaving 106.
- `glibc-2.33-libpthread-symbols-x86_64.txt`: the same 152 symbols plus
  `pthread_atfork`, minus the same 46, leaving 107.
- `glibc-2.33-librt-symbols.txt`: 25 symbols, identical on both architectures.
- `glibc-2.33-libdl-symbols.txt`: 6 symbols, identical on both architectures.
- `glibc-2.33-libutil-symbols.txt`: 3 symbols, identical on both architectures.

libanl has no symbols in common with musl. It is also not one of the library
names that musl aliases to its loader/libc DSO, so there is no `libanl.so.1`
facade. musl's reserved aliases also include libxnet, but glibc has no
corresponding compatibility DSO or symbol list here.

musl implements these aliases in `load_library()` in `ldso/dynlink.c`; its
reserved list is `c.pthread.rt.m.dl.util.xnet.`. A matching `DT_NEEDED` name
therefore resolves to musl's loader/libc DSO rather than loading another file.
