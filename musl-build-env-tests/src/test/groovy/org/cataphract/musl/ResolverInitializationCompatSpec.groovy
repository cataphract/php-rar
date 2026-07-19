package org.cataphract.musl

class ResolverInitializationCompatSpec extends CrossLibcSpecification {
    def 'res_init initializes a usable resolver on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/resolver_initialization_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'res_init ok'
        results.musl.stdout.trim() == 'res_init ok'
    }
}
