# Native, wrapper-independent CMake policy for the musl build image.
set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_ASM_COMPILER /usr/bin/clang)

set(CMAKE_AR /usr/bin/llvm-ar)
set(CMAKE_NM /usr/bin/llvm-nm)
set(CMAKE_RANLIB /usr/bin/llvm-ranlib)
set(CMAKE_STRIP /usr/bin/strip)

set(CMAKE_C_FLAGS_INIT "-fno-omit-frame-pointer")
# Clang applies -static-libstdc++ to the selected C++ standard library, libc++.
# It is a link-only option, hence -Qunused-arguments for compile-only commands.
set(CMAKE_CXX_FLAGS_INIT
    "-stdlib=libc++ -static-libstdc++ -fno-omit-frame-pointer -Qunused-arguments")

# Prefer static user libraries, then restore dynamic mode before Clang adds its
# implicit compiler runtimes and libc. libc.so itself injects compatibility
# archives while retaining the host musl DSO as the runtime dependency.
set(toolchain_link_flags
    "-rtlib=compiler-rt -unwindlib=libunwind -Wl,--gc-sections -Wl,-Bstatic")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${toolchain_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${toolchain_link_flags}")
set(CMAKE_C_STANDARD_LIBRARIES "-Wl,-Bdynamic")
set(CMAKE_CXX_STANDARD_LIBRARIES "-Wl,-Bdynamic")

# Tell CMake about the linker's initial static state. It can then bracket an
# explicit shared target with -Bdynamic and return to static lookup for later
# -l dependencies, instead of trying to link a shared object statically.
set(CMAKE_LINK_SEARCH_START_STATIC TRUE)
set(CMAKE_LINK_SEARCH_END_STATIC FALSE)

set(ZLIB_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
