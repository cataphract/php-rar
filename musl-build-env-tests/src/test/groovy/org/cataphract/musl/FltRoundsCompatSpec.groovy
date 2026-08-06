package org.cataphract.musl

class FltRoundsCompatSpec extends CrossLibcSpecification {
    def '__flt_rounds reports every standard floating-point rounding mode'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/flt_rounds_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslExecutableDependencies.contains('libm.so.6')
        results.glibc.stdout.trim() == 'flt-rounds-ok'
        results.musl.stdout.trim() == 'flt-rounds-ok'
    }
}
