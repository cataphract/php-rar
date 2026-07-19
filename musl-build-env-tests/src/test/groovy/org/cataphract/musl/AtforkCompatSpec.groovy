package org.cataphract.musl

class AtforkCompatSpec extends CrossLibcSpecification {
    def 'pthread_atfork handlers run in the correct processes on both libcs'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/atfork_compat.c', ['-pthread'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'pthread_atfork ok'
        results.musl.stdout.trim() == 'pthread_atfork ok'
    }
}
