package org.cataphract.musl

class MathCompatSpec extends CrossLibcSpecification {
    def 'ceil and ceilf work on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/math_compat.c',
            ['-fno-builtin-ceil', '-fno-builtin-ceilf'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'ceil ceilf ok'
        results.musl.stdout.trim() == 'ceil ceilf ok'
    }
}
