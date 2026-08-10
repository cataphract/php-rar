# musl-build-env cross-libc tests

This Gradle project compiles every sample inside a selected `musl-build-env`
image and runs the resulting executable in two clean runtime containers:

- glibc: `ubuntu:26.04`
- musl: `alpine:3.24`

The harness requires Docker and Java 17 or newer.

The runtime images and the build image are configurable:

```sh
./gradlew test \
  -PbuildEnvImage=musl-build-env:latest \
  -PglibcImage=ubuntu:26.04 \
  -PmuslImage=alpine:3.24
```

The Docker host and all three images must use the same CPU architecture. The
suite patches a copy of each executable's interpreter for the glibc run. The
linker wrapper already records the portable `libc.so.6` dependency and any
separate compatibility DSOs needed by glibc before 2.34. The original
executable is used unchanged for the musl run, and shared libraries are used
unchanged on both.

Runtime containers use an unconfined seccomp profile because x86_64 MSan must
call `personality(ADDR_NO_RANDOMIZE)` during startup. The default Docker seccomp
profile blocks that call.

`test` is deliberately a positive, fast suite. To establish that an individual
source patch is necessary, build a temporary `musl-build-env` image without
that patch and run the relevant spec against it with `-PbuildEnvImage=...`.

The narrower `patchTests` and `compatTests` tasks select specs whose class names
end in `PatchSpec` and `CompatSpec`, respectively.
