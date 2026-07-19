package org.cataphract.musl

class AlltypesPatchSpec extends CrossLibcSpecification {
    def 'pthread object layouts and operations work with both libc implementations'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/pthread-types-abi.c', ['-pthread'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'pthread ABI accepted'
        results.musl.stdout.trim() == 'pthread ABI accepted'
    }
}
