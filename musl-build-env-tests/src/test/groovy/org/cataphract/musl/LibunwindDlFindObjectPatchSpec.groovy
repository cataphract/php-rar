package org.cataphract.musl

class LibunwindDlFindObjectPatchSpec extends CrossLibcSpecification {
    def 'libunwind finds and traverses exception frames on both libc implementations'() {
        when:
        CompiledProgram program = harness.compileCpp(
            'samples/libunwind-exceptions.cpp', ['-std=c++20'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'exception unwound through 8 frames'
        results.musl.stdout.trim() == 'exception unwound through 8 frames'
    }
}
