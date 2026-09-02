#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>

/*
 * Runtime resolver for php_pcre2_* symbols.
 *
 * PHP loads extensions with RTLD_DEEPBIND. Extensions compiled with
 * -DHAVE_BUNDLED_PCRE call php_pcre2_* symbols that must resolve to either:
 * - PHP's bundled PCRE2 exports (php_pcre2_*), if PHP was built with the
 *   bundled PCRE2, or
 * - The system libpcre2-8 exports (pcre2_*_8), if PHP uses external PCRE2.
 *
 * The dlopen/dlsym dance is needed because PHP unfortunately uses
 * RTLD_DEEPBIND. Consequently, we can't just define php_pcre2_* calling a
 * (weak-imported) pcre2_*_8 and rely on symbol preemption from the main
 * executable.
 */
#define PCRE2_SYMS \
    X(callout_enumerate) \
    X(code_copy) \
    X(code_copy_with_tables) \
    X(code_free) \
    X(compile) \
    X(compile_context_copy) \
    X(compile_context_create) \
    X(compile_context_free) \
    X(config) \
    X(convert_context_copy) \
    X(convert_context_create) \
    X(convert_context_free) \
    X(dfa_match) \
    X(general_context_copy) \
    X(general_context_create) \
    X(general_context_free) \
    X(get_error_message) \
    X(get_mark) \
    X(get_ovector_count) \
    X(get_ovector_pointer) \
    X(get_startchar) \
    X(jit_compile) \
    X(jit_free_unused_memory) \
    X(jit_match) \
    X(jit_stack_assign) \
    X(jit_stack_create) \
    X(jit_stack_free) \
    X(maketables) \
    X(match) \
    X(match_context_copy) \
    X(match_context_create) \
    X(match_context_free) \
    X(match_data_create) \
    X(match_data_create_from_pattern) \
    X(match_data_free) \
    X(pattern_info) \
    X(serialize_decode) \
    X(serialize_encode) \
    X(serialize_free) \
    X(serialize_get_number_of_codes) \
    X(set_bsr) \
    X(set_callout) \
    X(set_character_tables) \
    X(set_compile_extra_options) \
    X(set_compile_recursion_guard) \
    X(set_depth_limit) \
    X(set_glob_escape) \
    X(set_glob_separator) \
    X(set_heap_limit) \
    X(set_match_limit) \
    X(set_max_pattern_length) \
    X(set_newline) \
    X(set_offset_limit) \
    X(set_parens_nest_limit) \
    X(set_recursion_limit) \
    X(set_recursion_memory_management) \
    X(substitute) \
    X(substring_copy_byname) \
    X(substring_copy_bynumber) \
    X(substring_free) \
    X(substring_get_byname) \
    X(substring_get_bynumber) \
    X(substring_length_byname) \
    X(substring_length_bynumber) \
    X(substring_list_free) \
    X(substring_list_get) \
    X(substring_nametable_scan) \
    X(substring_number_from_name)

#define X(name) __attribute__((visibility("hidden"))) void *_pcre2_compat_##name##_fn;
PCRE2_SYMS
#undef X

/*
 * Forward declaration of our own php_pcre2_match stub (defined in
 * pcre2_compat.S). Used as a sentinel to distinguish our stub from a real
 * php_pcre2_match exported by PHP's bundled PCRE2. This is assuming the local
 * stub won't be preempted (due to RTLD_DEEPBIND).
 */
extern void php_pcre2_match(void);

__attribute__((constructor))
static void pcre2_compat_resolve(void)
{
    /*
     * Detect whether PHP was built with bundled PCRE2 (exports php_pcre2_*)
     * or external PCRE2 (does not export php_pcre2_*).
     */
    void *main_handle = dlopen(NULL, RTLD_LAZY);
    void *found = main_handle ? dlsym(main_handle, "php_pcre2_match") : NULL;

    if (found && found != (void *)&php_pcre2_match) {
        /* Bundled PCRE2: php_pcre2_* are exported by the PHP binary. */
#define X(name) _pcre2_compat_##name##_fn = dlsym(main_handle, "php_pcre2_" #name);
        PCRE2_SYMS
#undef X
    } else {
        /* External PCRE2: resolve pcre2_*_8 from the global scope. */
#define X(name) _pcre2_compat_##name##_fn = dlsym(RTLD_DEFAULT, "pcre2_" #name "_8");
        PCRE2_SYMS
#undef X
    }
}
