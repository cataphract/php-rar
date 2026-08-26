package org.cataphract.musl

class LegacyLibrariesCompatSpec extends CrossLibcSpecification {
    private static final List<String> SPLIT_DEPENDENCIES = [
        'libpthread.so.0',
        'librt.so.1',
        'libm.so.6',
        'libdl.so.2',
        'libutil.so.1',
    ]

    def '#dependency is recorded for a pre-2.34 glibc library reference'() {
        when:
        CompiledProgram program = harness.compileC(
            "samples/${sample}", compilerArguments)
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        // These executable samples also import ordinary libc symbols.
        program.muslExecutableDependencies.contains('libc.so.6')
        program.muslExecutableDependencies.intersect(SPLIT_DEPENDENCIES) ==
            [dependency]
        !program.muslExecutableDependencies.any {
            it.startsWith('libc.musl-')
        }
        !program.muslExecutableDependencies.contains('libanl.so.1')
        results.glibc.stdout.trim() == expectedOutput
        results.musl.stdout.trim() == expectedOutput

        where:
        dependency       | sample                       | compilerArguments | expectedOutput
        'libpthread.so.0' | 'legacy_libpthread_compat.c' | ['-pthread']      | 'libpthread ok'
        'librt.so.1'      | 'legacy_librt_compat.c'      | []                | 'librt ok'
        'libm.so.6'       | 'math_compat.c'              | ['-fno-builtin']  | 'ceil ceilf ok'
        'libdl.so.2'      | 'legacy_libdl_compat.c'      | []                | 'libdl ok'
        'libutil.so.1'    | 'legacy_libutil_compat.c'    | []                | 'libutil ok'
    }

    def 'libc wins for symbols also exported by split libraries'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/libc_split_overlap_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslExecutableDependencies.contains('libc.so.6')
        program.muslExecutableDependencies.intersect(SPLIT_DEPENDENCIES) == []
        !program.muslExecutableDependencies.any {
            it.startsWith('libc.musl-')
        }
        results.glibc.stdout.trim() == 'libc overlap ok'
        results.musl.stdout.trim() == 'libc overlap ok'
    }

    def '#pthreadOption forces pthread ahead of libc for overlapping symbols'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/libc_split_overlap_compat.c', compilerArguments)
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslExecutableDependencies.intersect(SPLIT_DEPENDENCIES) ==
            ['libpthread.so.0']
        program.muslExecutableDependencies.indexOf('libpthread.so.0') <
            program.muslExecutableDependencies.indexOf('libc.so.6')
        results.glibc.stdout.trim() == 'libc overlap ok'
        results.musl.stdout.trim() == 'libc overlap ok'

        where:
        pthreadOption | compilerArguments
        '-pthread'     | ['-pthread']
        '-lpthread'    | ['-lpthread']
    }

    def '#mathOption forces math ahead of libc for overlapping symbols'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/libc_split_overlap_compat.c',
            ['-fno-builtin'] + compilerArguments)
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslExecutableDependencies.intersect(SPLIT_DEPENDENCIES) ==
            ['libm.so.6']
        program.muslExecutableDependencies.indexOf('libm.so.6') <
            program.muslExecutableDependencies.indexOf('libc.so.6')
        results.glibc.stdout.trim() == 'libc overlap ok'
        results.musl.stdout.trim() == 'libc overlap ok'

        where:
        mathOption | compilerArguments
        '-lm'       | ['-lm']
        '-l m'      | ['-l', 'm']
    }

    def 'multiple split dependencies have deterministic unique ordering'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/multi_library_compat.c', ['-pthread', '-fno-builtin'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslExecutableDependencies ==
            SPLIT_DEPENDENCIES + ['libc.so.6']
    }
}
