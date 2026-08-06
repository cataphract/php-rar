# glibc compatibility symbol lists

The linker wrapper compares an output's undefined dynamic symbols with static
intersections of symbols exported by musl and by glibc compatibility DSOs.

`glibc-libm-symbols.txt` is the amd64 and arm64 intersection from
`alpine:latest` (Alpine 3.24.1, musl 1.2.6) and `libm.so.6` from Ubuntu 26.04
(glibc 2.43-2ubuntu2). The two architectures have the same 288 symbols.

glibc 2.34 integrated libpthread, libdl, libutil, and libanl into libc. The
legacy lists use Fedora 34's glibc 2.33-21.fc34 DSOs and the musl 1.2.5-r23
used by this build environment:

- `glibc-2.33-libpthread-symbols-aarch64.txt`: 152 symbols.
- `glibc-2.33-libpthread-symbols-x86_64.txt`: the same symbols plus
  `pthread_atfork`.
- `glibc-2.33-librt-symbols.txt`: 25 symbols, identical on both architectures.
- `glibc-2.33-libdl-symbols.txt`: 6 symbols, identical on both architectures.
- `glibc-2.33-libutil-symbols.txt`: 3 symbols, identical on both architectures.

libanl has no symbols in common with musl. It is also not one of the library
names that musl aliases to its loader/libc DSO, so the wrapper must not add a
`libanl.so.1` dependency. musl's reserved aliases also include libxnet, but
glibc has no corresponding compatibility DSO or symbol list here.

musl implements these aliases in `load_library()` in `ldso/dynlink.c`; its
reserved list is `c.pthread.rt.m.dl.util.xnet.`. A matching `DT_NEEDED` name
therefore resolves to musl's loader/libc DSO rather than loading another file.
