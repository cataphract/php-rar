package org.cataphract.musl

class FreadaheadCompatSpec extends CrossLibcSpecification {
    def '__freadahead reports bytes remaining in a buffered FILE on both libc implementations'() {
        when:
        CompiledProgram program = harness.compileC('samples/freadahead-buffer.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'freadahead counts=63,56,0'
        results.musl.stdout.trim() == 'freadahead counts=63,56,0'
    }
}
