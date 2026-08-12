package org.cataphract.musl.sanitizer.normal

import org.cataphract.musl.sanitizer.MuslSanitizerProgram
import org.cataphract.musl.sanitizer.MuslSanitizerSpecification

class CompilerRtRlimit64PatchSpec extends MuslSanitizerSpecification {
    def 'MSan resource limit interceptors run on musl'() {
        when:
        MuslSanitizerProgram program = harness.compileC(
            'samples/compiler_rt_rlimit64.c',
            ['-O1', '-fsanitize=memory', '-fPIE', '-pie'])
        def result = harness.run(program)

        then:
        result.exitCode == 0
        result.stdout.trim() == 'msan-rlimit64-ok'
    }
}
