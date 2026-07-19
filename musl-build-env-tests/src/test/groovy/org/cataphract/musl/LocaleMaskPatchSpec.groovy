package org.cataphract.musl

class LocaleMaskPatchSpec extends CrossLibcSpecification {
    def 'LC_ALL_MASK is accepted by both libc locale implementations'() {
        when:
        CompiledProgram program = harness.compileC('samples/locale-mask.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'locale mask accepted'
        results.musl.stdout.trim() == 'locale mask accepted'
    }
}
