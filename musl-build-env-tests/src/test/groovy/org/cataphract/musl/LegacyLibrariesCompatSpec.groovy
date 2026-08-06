package org.cataphract.musl

class LegacyLibrariesCompatSpec extends CrossLibcSpecification {
    def '#dependency is recorded for a pre-2.34 glibc library reference'() {
        when:
        CompiledProgram program = harness.compileC(
            "samples/${sample}", compilerArguments)
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslExecutableDependencies.contains(dependency)
        !program.muslExecutableDependencies.contains('libanl.so.1')
        results.glibc.stdout.trim() == expectedOutput
        results.musl.stdout.trim() == expectedOutput

        where:
        dependency       | sample                       | compilerArguments | expectedOutput
        'libpthread.so.0' | 'legacy_libpthread_compat.c' | ['-pthread']      | 'libpthread ok'
        'librt.so.1'      | 'legacy_librt_compat.c'      | []                | 'librt ok'
        'libdl.so.2'      | 'legacy_libdl_compat.c'      | []                | 'libdl ok'
        'libutil.so.1'    | 'legacy_libutil_compat.c'    | []                | 'libutil ok'
    }
}
