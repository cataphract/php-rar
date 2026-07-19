package org.cataphract.musl

import spock.lang.Unroll

class CompilerRtStatPatchSpec extends CrossLibcSpecification {
    @Unroll
    def 'MSan #functionName interceptor unpoisons results on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/compiler_rt_stat.c',
            ['-O1', '-fsanitize=memory', '-fPIE', '-pie'])
        CrossLibcResults results = harness.runOnBoth(program, [functionName])

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == "msan-${functionName}-ok"
        results.musl.stdout.trim() == "msan-${functionName}-ok"

        where:
        functionName << [
            'stat', '__xstat',
            'fstat', '__fxstat',
            'lstat', '__lxstat',
            'fstatat', '__fxstatat'
        ]
    }
}
