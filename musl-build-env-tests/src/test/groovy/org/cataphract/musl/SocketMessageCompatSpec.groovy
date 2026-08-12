package org.cataphract.musl

import spock.lang.Unroll

class SocketMessageCompatSpec extends CrossLibcSpecification {
    private static final List<List<String>> OPERATIONS = [
        ['sendmsg', 'samples/socket_message_sendmsg.c'],
        ['recvmsg', 'samples/socket_message_recvmsg.c'],
        ['sendmmsg', 'samples/socket_message_sendmmsg.c'],
        ['recvmmsg', 'samples/socket_message_recvmmsg.c'],
    ]

    @Unroll
    def '#operation translates musl socket-message layouts'() {
        when:
        CompiledProgram program = harness.compileC(sourceResource)
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == "${operation} padding ok"
        results.musl.stdout.trim() == "${operation} padding ok"

        where:
        [operation, sourceResource] << OPERATIONS
    }

    @Unroll
    def '#operation translates musl socket-message layouts from a shared library'() {
        when:
        CompiledProgram program = harness.compileCWithSharedLibrary(
            'samples/socket_message_dso_loader.c', sourceResource)
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        program.muslSharedLibraryDependencies.contains('libc.so.6')
        !program.muslSharedLibraryDependencies.any {
            it.startsWith('libc.musl-')
        }
        results.glibc.stdout.trim() == "${operation} padding ok"
        results.musl.stdout.trim() == "${operation} padding ok"

        where:
        [operation, sourceResource] << OPERATIONS
    }
}
