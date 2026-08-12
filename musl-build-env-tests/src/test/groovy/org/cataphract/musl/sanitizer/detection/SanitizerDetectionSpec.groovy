package org.cataphract.musl.sanitizer.detection

import org.cataphract.musl.sanitizer.MuslSanitizerProgram
import org.cataphract.musl.sanitizer.MuslSanitizerSpecification
import spock.lang.Unroll

class SanitizerDetectionSpec extends MuslSanitizerSpecification {
    @Unroll
    def '#sanitizer detects #bugCategory'() {
        when:
        MuslSanitizerProgram program = harness.compileC(
            'samples/sanitizer_detection.c', compilerArguments)
        def result = harness.run(program)

        then:
        result.exitCode != 0
        result.stderr.contains(sanitizerReport)
        result.stderr.contains(bugReport)

        where:
        [sanitizer, bugCategory, compilerArguments,
         sanitizerReport, bugReport] << [
            [
                'ASan', 'a heap use after free',
                addressArguments('USE_ASAN'),
                'AddressSanitizer', 'heap-use-after-free',
            ],
            [
                'LSan', 'an unreachable allocation', leakArguments(),
                'LeakSanitizer', 'detected memory leaks',
            ],
            [
                'MSan', 'an uninitialized read', memoryArguments(),
                'MemorySanitizer', 'use-of-uninitialized-value',
            ],
            [
                'UBSan', 'signed integer overflow', undefinedArguments(),
                'runtime error', 'signed integer overflow',
            ],
        ]
    }

    def 'ASan sendmsg interceptor detects poisoned application memory'() {
        when:
        MuslSanitizerProgram program = harness.compileC(
            'samples/asan_sendmsg_detection.c',
            ['-O1', '-g', '-fsanitize=address'])
        def result = harness.run(program)

        then:
        result.exitCode != 0
        result.stderr.contains('AddressSanitizer')
        result.stderr.contains('use-after-poison')
        program.hasWeakFunctionSymbol('sendmsg')
        !program.hasHiddenFunctionSymbol('sendmsg')
    }

    private static List<String> addressArguments(String define) {
        return ['-O1', '-g', '-fsanitize=address', "-D${define}"]
    }

    private static List<String> leakArguments() {
        return ['-O1', '-g', '-fsanitize=leak', '-DUSE_LSAN']
    }

    private static List<String> memoryArguments() {
        return [
            '-O1', '-g', '-fsanitize=memory', '-fPIE', '-pie', '-DUSE_MSAN',
        ]
    }

    private static List<String> undefinedArguments() {
        return [
            '-O1', '-g', '-fsanitize=undefined',
            '-fno-sanitize-recover=undefined', '-DUSE_UBSAN',
        ]
    }

}
