package org.cataphract.musl

class SigsetjmpCompatSpec extends CrossLibcSpecification {
    def 'sigsetjmp restores control flow locals and signal mask on both libc implementations'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/sigsetjmp-control-flow.c', ['-O0'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() ==
            'sigsetjmp restored value=73 marker=unchanged mask=blocked'
        results.musl.stdout.trim() ==
            'sigsetjmp restored value=73 marker=unchanged mask=blocked'
    }
}
