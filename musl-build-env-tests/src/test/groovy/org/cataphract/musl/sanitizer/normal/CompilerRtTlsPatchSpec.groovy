package org.cataphract.musl.sanitizer.normal

import org.cataphract.musl.sanitizer.MuslSanitizerProgram
import org.cataphract.musl.sanitizer.MuslSanitizerSpecification

class CompilerRtTlsPatchSpec extends MuslSanitizerSpecification {
    def 'LSan treats live pthread-specific values as roots on musl'() {
        when:
        MuslSanitizerProgram program = harness.compileCWithSharedLibrary(
            'samples/compiler_rt_tls.c',
            'samples/compiler_rt_tls_module.c',
            ['-O1', '-g', '-fsanitize=leak', '-pthread'])
        def result = harness.run(program)

        then:
        result.exitCode == 0
        result.stdout.trim() == 'lsan-pthread-specific-ok'
    }
}
