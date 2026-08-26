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
        !program.muslSharedLibraryDependencies.any {
            it.startsWith('libc.musl-')
        }
        program.muslSharedLibraryDependencies.contains('libm.so.6')
        results.glibc.stdout.trim() == 'shared ceil ceilf ok'
        results.musl.stdout.trim() == 'shared ceil ceilf ok'
    }

    def 'signgam selects libm without a copy relocation'() {
        when:
        CompiledProgram program = harness.compileC('samples/signgam_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)
        String symbols = harness.buildEnvironmentOutput(
            'read signgam symbols',
            ['readelf', '--dyn-syms', '--wide',
             program.buildContainerExecutable])
        String relocations = harness.buildEnvironmentOutput(
            'read signgam relocations',
            ['readelf', '--relocs', '--wide',
             program.buildContainerExecutable])

        then:
        results.assertSuccess()
        program.muslExecutableDependencies.contains('libm.so.6')
        program.muslExecutableDependencies.contains('libc.so.6')
        symbols.readLines().any {
            it.contains(' UND ') && it.trim().endsWith(' signgam')
        }
        !relocations.contains('COPY')
    }
}
