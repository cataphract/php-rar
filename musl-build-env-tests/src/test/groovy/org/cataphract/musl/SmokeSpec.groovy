package org.cataphract.musl

import org.testcontainers.containers.Container

class SmokeSpec extends CrossLibcSpecification {
    def 'a program built by musl-build-env runs on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC('samples/smoke.c')
        CrossLibcResults results = harness.runOnBoth(program)
        String notes = harness.buildEnvironmentOutput(
            'read the build ID',
            ['readelf', '--notes', '--wide',
             program.buildContainerExecutable])
        String symbols = harness.buildEnvironmentOutput(
            'read dynamic symbols',
            ['readelf', '--syms', '--wide',
             program.buildContainerExecutable])
        String segments = harness.buildEnvironmentOutput(
            'read program headers',
            ['readelf', '--segments', '--wide',
             program.buildContainerExecutable])

        then:
        results.assertSuccess()
        program.muslExecutableDependencies.contains('libc.so.6')
        !program.muslExecutableDependencies.any {
            it.startsWith('libc.musl-')
        }
        program.muslExecutableDependencies.intersect([
            'libpthread.so.0',
            'librt.so.1',
            'libm.so.6',
            'libdl.so.2',
            'libutil.so.1',
        ]).isEmpty()
        notes.contains('Build ID:')
        dynamicAddress(symbols) == dynamicSegmentAddress(segments)
        results.glibc.stdout.trim() == 'hello from musl-build-env'
        results.musl.stdout.trim() == 'hello from musl-build-env'
    }

    def 'non-dynamic link modes do not acquire dynamic dependencies'() {
        when:
        Container.ExecResult result = harness.buildEnvironmentCommand([
            'sh', '-ceu', '''
                work=$(mktemp -d)
                trap 'rm -rf "$work"' EXIT HUP INT TERM
                printf '%s\n' \
                    'void probe(void) {}' > "$work/probe.c"
                printf '%s\n' \
                    '#include <stdio.h>' \
                    'int main(void) { puts("static"); return 0; }' \
                    > "$work/main.c"
                musl-clang -c "$work/probe.c" -o "$work/probe.o"
                musl-clang -nostdlib -pthread -Wl,-e,probe "$work/probe.o" \
                    -o "$work/nostdlib"
                musl-clang -nodefaultlibs -nostartfiles -pthread -Wl,-e,probe \
                    "$work/probe.o" -o "$work/nodefaultlibs"
                musl-clang -static -pthread -lm "$work/main.c" \
                    -o "$work/static"
                musl-clang -static-pie -pthread -lm "$work/main.c" \
                    -o "$work/static-pie"
                ld.lld -r "$work/probe.o" -o "$work/relocatable.o"
                for output in "$work/nostdlib" "$work/nodefaultlibs" \
                    "$work/static" "$work/static-pie" \
                    "$work/relocatable.o"; do
                    ! readelf -d "$output" 2>/dev/null | grep -q '(NEEDED)'
                done
            '''.stripIndent(),
        ])

        then:
        result.exitCode == 0
    }

    private static BigInteger dynamicAddress(String symbols) {
        String line = symbols.readLines().find {
            it.trim().endsWith(' _DYNAMIC')
        }
        assert line != null
        return new BigInteger(line.trim().split(/\s+/)[1], 16)
    }

    private static BigInteger dynamicSegmentAddress(String segments) {
        String line = segments.readLines().find {
            it.trim().startsWith('DYNAMIC ')
        }
        assert line != null
        String value = line.trim().split(/\s+/)[2].replaceFirst('^0x', '')
        return new BigInteger(value, 16)
    }
}
