package org.cataphract.musl

class SystemInterfacesCompatSpec extends CrossLibcSpecification {
    def 'strerror_r getrandom and memfd_create work on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/system_interfaces_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'strerror_r getrandom memfd_create ok'
        results.musl.stdout.trim() == 'strerror_r getrandom memfd_create ok'
    }
}
