package org.cataphract.musl

class ConditionVariableCompatSpec extends CrossLibcSpecification {
    def 'a monotonic condition variable works on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/condition_variable_compat.c', ['-pthread'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'pthread_cond_init ok'
        results.musl.stdout.trim() == 'pthread_cond_init ok'
    }
}
