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
        program.muslExecutableDependencies.contains('libm.so.6')
        results.glibc.stdout.trim() == 'ceil ceilf ok'
        results.musl.stdout.trim() == 'ceil ceilf ok'
    }

    def 'a shared library that references libm symbols runs on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileCWithSharedLibrary(
            'samples/math_dso_loader.c',
            'samples/math_dso_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslSharedLibraryDependencies.contains('libm.so.6')
        results.glibc.stdout.trim() == 'shared ceil ceilf ok'
        results.musl.stdout.trim() == 'shared ceil ceilf ok'
    }
}
