/*
 * Assembly stubs for php_pcre2_* symbol bridging.
 *
 * Each stub is a global php_pcre2_NAME that tail-calls through the function
 * pointer _pcre2_compat_NAME_fn resolved at load time by the constructor in
 * pcre2_compat.c.
 */
    .text

#ifdef __x86_64__

.macro pcre2_compat name
    .globl  php_pcre2_\name
    .type   php_pcre2_\name, @function
php_pcre2_\name:
    jmp     *_pcre2_compat_\name\()_fn(%rip)
    .size   php_pcre2_\name, . - php_pcre2_\name
.endm

#elif defined(__aarch64__)

.macro pcre2_compat name
    .globl  php_pcre2_\name
    .type   php_pcre2_\name, @function
php_pcre2_\name:
    adrp    x16, _pcre2_compat_\name\()_fn
    ldr     x16, [x16, :lo12:_pcre2_compat_\name\()_fn]
    br      x16
    .size   php_pcre2_\name, . - php_pcre2_\name
.endm

#endif /* __x86_64__ / __aarch64__ */

    pcre2_compat callout_enumerate
    pcre2_compat code_copy
    pcre2_compat code_copy_with_tables
    pcre2_compat code_free
    pcre2_compat compile
    pcre2_compat compile_context_copy
    pcre2_compat compile_context_create
    pcre2_compat compile_context_free
    pcre2_compat config
    pcre2_compat convert_context_copy
    pcre2_compat convert_context_create
    pcre2_compat convert_context_free
    pcre2_compat dfa_match
    pcre2_compat general_context_copy
    pcre2_compat general_context_create
    pcre2_compat general_context_free
    pcre2_compat get_error_message
    pcre2_compat get_mark
    pcre2_compat get_ovector_count
    pcre2_compat get_ovector_pointer
    pcre2_compat get_startchar
    pcre2_compat jit_compile
    pcre2_compat jit_free_unused_memory
    pcre2_compat jit_match
    pcre2_compat jit_stack_assign
    pcre2_compat jit_stack_create
    pcre2_compat jit_stack_free
    pcre2_compat maketables
    pcre2_compat match
    pcre2_compat match_context_copy
    pcre2_compat match_context_create
    pcre2_compat match_context_free
    pcre2_compat match_data_create
    pcre2_compat match_data_create_from_pattern
    pcre2_compat match_data_free
    pcre2_compat pattern_info
    pcre2_compat serialize_decode
    pcre2_compat serialize_encode
    pcre2_compat serialize_free
    pcre2_compat serialize_get_number_of_codes
    pcre2_compat set_bsr
    pcre2_compat set_callout
    pcre2_compat set_character_tables
    pcre2_compat set_compile_extra_options
    pcre2_compat set_compile_recursion_guard
    pcre2_compat set_depth_limit
    pcre2_compat set_glob_escape
    pcre2_compat set_glob_separator
    pcre2_compat set_heap_limit
    pcre2_compat set_match_limit
    pcre2_compat set_max_pattern_length
    pcre2_compat set_newline
    pcre2_compat set_offset_limit
    pcre2_compat set_parens_nest_limit
    pcre2_compat set_recursion_limit
    pcre2_compat set_recursion_memory_management
    pcre2_compat substitute
    pcre2_compat substring_copy_byname
    pcre2_compat substring_copy_bynumber
    pcre2_compat substring_free
    pcre2_compat substring_get_byname
    pcre2_compat substring_get_bynumber
    pcre2_compat substring_length_byname
    pcre2_compat substring_length_bynumber
    pcre2_compat substring_list_free
    pcre2_compat substring_list_get
    pcre2_compat substring_nametable_scan
    pcre2_compat substring_number_from_name
