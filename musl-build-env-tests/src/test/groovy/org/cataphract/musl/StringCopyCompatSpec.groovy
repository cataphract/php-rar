package org.cataphract.musl

class StringCopyCompatSpec extends CrossLibcSpecification {
    def 'strlcpy and strlcat work on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/string_copy_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'strlcpy strlcat ok'
        results.musl.stdout.trim() == 'strlcpy strlcat ok'
    }
}
