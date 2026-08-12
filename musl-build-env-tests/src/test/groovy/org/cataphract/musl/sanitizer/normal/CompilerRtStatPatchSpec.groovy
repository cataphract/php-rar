package org.cataphract.musl.sanitizer.normal

import org.cataphract.musl.sanitizer.MuslSanitizerProgram
import org.cataphract.musl.sanitizer.MuslSanitizerSpecification
import spock.lang.Unroll

class CompilerRtStatPatchSpec extends MuslSanitizerSpecification {
    @Unroll
    def 'MSan #functionName interceptor unpoisons results on musl'() {
        when:
        MuslSanitizerProgram program = harness.compileC(
            'samples/compiler_rt_stat.c',
            ['-O1', '-fsanitize=memory', '-fPIE', '-pie'])
        def result = harness.run(program, [functionName])

        then:
        result.exitCode == 0
        result.stdout.trim() == "msan-${functionName}-ok"

        where:
        functionName << ['stat', 'fstat', 'lstat', 'fstatat']
    }
}
