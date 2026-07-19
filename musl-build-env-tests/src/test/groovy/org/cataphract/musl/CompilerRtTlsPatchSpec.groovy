package org.cataphract.musl

class CompilerRtTlsPatchSpec extends CrossLibcSpecification {
    def 'LSan treats live pthread-specific values as roots on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileCWithSharedLibrary(
            'samples/compiler_rt_tls.c',
            'samples/compiler_rt_tls_module.c',
            ['-O1', '-g', '-fsanitize=leak', '-pthread'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'lsan-pthread-specific-ok'
        results.musl.stdout.trim() == 'lsan-pthread-specific-ok'
    }
}
