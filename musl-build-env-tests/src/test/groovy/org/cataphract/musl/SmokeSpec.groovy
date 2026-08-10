package org.cataphract.musl

class SmokeSpec extends CrossLibcSpecification {
    def 'a program built by musl-build-env runs on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC('samples/smoke.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslExecutableDependencies.contains('libc.so.6')
        !program.muslExecutableDependencies.any {
            it.startsWith('libc.musl-')
        }
        program.muslExecutableDependencies.intersect([
            'libpthread.so.0',
            'librt.so.1',
            'libm.so.6',
            'libdl.so.2',
            'libutil.so.1',
        ]).isEmpty()
        results.glibc.stdout.trim() == 'hello from musl-build-env'
        results.musl.stdout.trim() == 'hello from musl-build-env'
    }
}
