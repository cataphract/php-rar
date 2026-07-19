package org.cataphract.musl

class CompilerRtRlimit64PatchSpec extends CrossLibcSpecification {
    def 'MSan resource limit interceptors run on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/compiler_rt_rlimit64.c',
            ['-O1', '-fsanitize=memory', '-fPIE', '-pie'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'msan-rlimit64-ok'
        results.musl.stdout.trim() == 'msan-rlimit64-ok'
    }
}
