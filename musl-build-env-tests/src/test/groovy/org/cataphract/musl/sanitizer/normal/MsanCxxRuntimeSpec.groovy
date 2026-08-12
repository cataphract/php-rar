package org.cataphract.musl.sanitizer.normal

import org.cataphract.musl.sanitizer.MuslSanitizerProgram
import org.cataphract.musl.sanitizer.MuslSanitizerSpecification

class MsanCxxRuntimeSpec extends MuslSanitizerSpecification {
    // Selecting the instrumented libc++ is a policy decision, so the wrapper
    // needs MUSL_CLANG_SANITIZE. Without it -static-libstdc++ wins and the link
    // takes the uninstrumented /usr/lib/libc++.a.
    def setupSpec() {
        harness.copyResourceToBuildEnvironment(
            'policies/msan-musl-clang.conf', '/etc/musl-clang.conf')
    }

    def 'MSan reports no false positive in the C++ standard library'() {
        when:
        MuslSanitizerProgram program = harness.compileCpp(
            'samples/msan_cxx_standard_library.cpp',
            ['-O1', '-g', '-fsanitize=memory', '-fPIE', '-pie'])
        def result = harness.run(program)

        then:
        result.exitCode == 0
        result.stdout.trim() == 'msan-cxx-standard-library-ok'

        and: 'the shared runtimes, not the plain static archives'
        program.runtimeLibraries.keySet().containsAll(
            ['libc++.so.1', 'libc++abi.so.1', 'libunwind.so.1'])
    }
}
