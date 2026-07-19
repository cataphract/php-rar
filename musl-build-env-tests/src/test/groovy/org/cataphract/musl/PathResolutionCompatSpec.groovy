package org.cataphract.musl

class PathResolutionCompatSpec extends CrossLibcSpecification {
    def 'realpath with an allocated result works on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/path_resolution_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'realpath ok'
        results.musl.stdout.trim() == 'realpath ok'
    }
}
