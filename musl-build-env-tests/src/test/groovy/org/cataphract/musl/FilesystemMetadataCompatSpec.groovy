package org.cataphract.musl

class FilesystemMetadataCompatSpec extends CrossLibcSpecification {
    def 'stat family and statx work on glibc and musl'() {
        when:
        CompiledProgram program = harness.compileC(
            'samples/filesystem_metadata_compat.c')
        CrossLibcResults results = harness.runOnBoth(program)

        then:
        results.assertSuccess()
        results.glibc.stdout.trim() == 'stat fstat lstat fstatat statx ok'
        results.musl.stdout.trim() == 'stat fstat lstat fstatat statx ok'
    }
}
