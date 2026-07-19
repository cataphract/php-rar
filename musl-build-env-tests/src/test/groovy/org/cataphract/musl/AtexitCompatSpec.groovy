package org.cataphract.musl

class AtexitCompatSpec extends CrossLibcSpecification {
    def 'atexit callbacks run in reverse registration order on both libc implementations'() {
        when:
        CompiledProgram program = harness.compileC('samples/atexit-callbacks.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout == 'main\nsecond callback\nfirst callback\n'
        results.musl.stdout == 'main\nsecond callback\nfirst callback\n'
    }
}
