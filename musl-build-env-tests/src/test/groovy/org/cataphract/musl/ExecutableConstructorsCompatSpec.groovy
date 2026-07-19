package org.cataphract.musl

class ExecutableConstructorsCompatSpec extends CrossLibcSpecification {
    def 'executable constructors and destructors run exactly once on both libc implementations'() {
        when:
        CompiledProgram program = harness.compileCpp(
            'samples/executable-constructors.cpp', ['-std=c++20'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout == 'constructor one\nconstructor two\nmain\ndestructor two\ndestructor one\n'
        results.musl.stdout == 'constructor one\nconstructor two\nmain\ndestructor two\ndestructor one\n'
    }
}
