# Native, wrapper-independent CMake policy for the musl build image.
set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_ASM_COMPILER /usr/bin/clang)

set(CMAKE_AR /usr/bin/llvm-ar)
set(CMAKE_NM /usr/bin/llvm-nm)
set(CMAKE_RANLIB /usr/bin/llvm-ranlib)
set(CMAKE_STRIP /usr/bin/strip)

# Derived images install /etc/musl-clang.conf to select a sanitizer policy. It
# is Bash, so source it once in Bash and return three newline-delimited values
# for CMake to unpack.
set(MUSL_COMPILE_FLAGS "")
set(MUSL_LINK_FLAGS "")
set(MUSL_SANITIZE "")
if(EXISTS "/etc/musl-clang.conf")
    set(_musl_prelude
        "MUSL_CLANG_COMPILE_FLAGS=(); MUSL_CLANG_LINK_FLAGS=(); . /etc/musl-clang.conf;")
    string(CONCAT _musl_read
        "${_musl_prelude} "
        "printf '%s\\n%s\\n%s' "
        "\"\${MUSL_CLANG_COMPILE_FLAGS[*]-}\" "
        "\"\${MUSL_CLANG_LINK_FLAGS[*]-}\" "
        "\"\${MUSL_CLANG_SANITIZE-}\"")
    execute_process(
        COMMAND bash -c "${_musl_read}"
        OUTPUT_VARIABLE _musl_values
        RESULT_VARIABLE _musl_status)
    if(NOT _musl_status EQUAL 0)
        message(FATAL_ERROR "Could not read /etc/musl-clang.conf")
    endif()
    string(REPLACE "\n" ";" _musl_values "${_musl_values}")
    list(GET _musl_values 0 MUSL_COMPILE_FLAGS)
    list(GET _musl_values 1 MUSL_LINK_FLAGS)
    list(GET _musl_values 2 MUSL_SANITIZE)
endif()

# Sanitizer compile flags must also reach the link command so the runtimes reach
# the executable. CMake includes CMAKE_<LANG>_FLAGS in its link rule, so seeding
# it here covers both.
#
# For ASan and MSan, the selected library directory goes first on every link,
# C and C++ alike. In sanitized C++ mode it also resolves libc++ to the
# instrumented DSO instead of the static one. Its reduced libc.so selects only
# the libc facade for native-musl execution. These are link-only options, hence
# -Qunused-arguments for compile-only commands.
#
# As in the wrappers, only MUSL_SANITIZE drives this selection; explicit
# -fsanitize=address or -fsanitize=memory flags do not.
set(_musl_sanitizer_lib_dir "")
set(_musl_cxx_runtime "-static-libstdc++")
if(MUSL_SANITIZE MATCHES "address")
    set(_musl_sanitizer_lib_dir "/usr/asan/lib")
    set(_musl_cxx_runtime "")
elseif(MUSL_SANITIZE MATCHES "memory")
    set(_musl_sanitizer_lib_dir "/usr/msan/lib")
    set(_musl_cxx_runtime "")
endif()
if(_musl_sanitizer_lib_dir)
    set(_musl_sanitizer_runtime
        "-L${_musl_sanitizer_lib_dir} -Wl,-rpath,${_musl_sanitizer_lib_dir}")
else()
    set(_musl_sanitizer_runtime "")
endif()
set(CMAKE_C_FLAGS_INIT
    "${_musl_sanitizer_runtime} -fno-omit-frame-pointer -Qunused-arguments ${MUSL_COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT
    "-stdlib=libc++ ${_musl_sanitizer_runtime} ${_musl_cxx_runtime} -fno-omit-frame-pointer -Qunused-arguments ${MUSL_COMPILE_FLAGS}")

# Prefer static user libraries, then restore dynamic mode before Clang adds its
# implicit compiler runtimes and libc. libc.so injects compatibility archives
# before selecting portable dependency names through linker-only facades.
set(toolchain_link_flags
    "-rtlib=compiler-rt -unwindlib=libunwind -Wl,--gc-sections -Wl,-Bstatic")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${toolchain_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${toolchain_link_flags}")

# CMAKE_<LANG>_STANDARD_LIBRARIES is appended last, so this toolchain puts its
# link-only policy there after restoring dynamic lookup.
set(CMAKE_C_STANDARD_LIBRARIES "-Wl,-Bdynamic ${MUSL_LINK_FLAGS}")
set(CMAKE_CXX_STANDARD_LIBRARIES "-Wl,-Bdynamic ${MUSL_LINK_FLAGS}")

# Tell CMake about the linker's initial static state. It can then bracket an
# explicit shared target with -Bdynamic and return to static lookup for later
# -l dependencies, instead of trying to link a shared object statically.
set(CMAKE_LINK_SEARCH_START_STATIC TRUE)
set(CMAKE_LINK_SEARCH_END_STATIC FALSE)

set(ZLIB_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
