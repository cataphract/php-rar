package org.cataphract.musl

class ExecutableDestructorsCompatSpec extends CrossLibcSpecification {
    def 'executable exit hooks match musl ordering on both libc implementations'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/executable-destructors.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout == results.musl.stdout
        results.musl.stdout in [
            'main\natexit\nfini two\nfini one\n',
            'main\natexit\nfini two\nfini one\nfini\n',
        ]
    }
}
