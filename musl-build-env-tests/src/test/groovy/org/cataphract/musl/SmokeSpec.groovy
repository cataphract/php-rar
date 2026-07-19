package org.cataphract.musl

class SmokeSpec extends CrossLibcSpecification {
    def 'a program built by musl-build-env runs on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC('samples/smoke.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'hello from musl-build-env'
        results.musl.stdout.trim() == 'hello from musl-build-env'
    }
}
