package org.cataphract.musl

class CompilerRtMsghdrPatchSpec extends CrossLibcSpecification {
    def 'ASan sendmsg and recvmsg interceptors understand musl field widths'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/compiler_rt_msghdr.c',
            ['-O1', '-fsanitize=address'])
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'asan-msghdr-ok'
        results.musl.stdout.trim() == 'asan-msghdr-ok'
    }
}
