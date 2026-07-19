package org.cataphract.musl

import com.github.dockerjava.api.command.CreateContainerCmd
import groovy.transform.CompileStatic
import org.testcontainers.containers.Container
import org.testcontainers.containers.GenericContainer
import org.testcontainers.utility.DockerImageName
import org.testcontainers.utility.MountableFile

import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.util.concurrent.atomic.AtomicBoolean
import java.util.function.Consumer

@CompileStatic
final class CrossLibcTestHarness implements Closeable {
    private static final CrossLibcTestHarness INSTANCE = fromSystemProperties()

    private final String buildEnvImage
    private final String glibcImage
    private final String muslImage
    private final Path transferDirectory
    private final AtomicBoolean started = new AtomicBoolean()
    private final AtomicBoolean closed = new AtomicBoolean()

    private GenericContainer<?> buildContainer
    private GenericContainer<?> glibcContainer
    private GenericContainer<?> muslContainer
    private String architecture

    private CrossLibcTestHarness(String buildEnvImage, String glibcImage,
                                String muslImage) {
        this.buildEnvImage = buildEnvImage
        this.glibcImage = glibcImage
        this.muslImage = muslImage
        this.transferDirectory = Files.createTempDirectory('musl-build-env-tests-')
    }

    static CrossLibcTestHarness shared() {
        return INSTANCE
    }

    synchronized void start() {
        if (started.get()) {
            return
        }
        if (closed.get()) {
            throw new IllegalStateException('The cross-libc harness is already closed')
        }

        buildContainer = longRunningContainer(buildEnvImage)
        glibcContainer = runtimeContainer(glibcImage)
        muslContainer = runtimeContainer(muslImage)

        try {
            buildContainer.start()
            architecture = successfulOutput(
                'detect the build image architecture',
                buildContainer.execInContainer('uname', '-m')
            ).trim()
            validateArchitecture(architecture)
            glibcContainer.start()
            muslContainer.start()
            started.set(true)
        } catch (Throwable failure) {
            close()
            throw failure
        }
    }

    CompiledProgram compileC(String classpathResource,
                             List<String> compilerArguments = []) {
        return compile(classpathResource, 'musl-clang', compilerArguments, null)
    }

    CompiledProgram compileCWithSharedLibrary(
        String classpathResource, String sharedLibraryResource,
        List<String> compilerArguments = []) {
        return compile(
            classpathResource, 'musl-clang', compilerArguments,
            sharedLibraryResource)
    }

    CompiledProgram compileCpp(String classpathResource,
                               List<String> compilerArguments = []) {
        return compile(classpathResource, 'musl-clang++', compilerArguments, null)
    }

    CrossLibcResults runOnBoth(CompiledProgram program,
                               List<String> arguments = []) {
        requireStarted()
        Container.ExecResult glibcResult = run(
            glibcContainer, program.glibcExecutable,
            program.glibcSharedLibrary, program.id, arguments)
        Container.ExecResult muslResult = run(
            muslContainer, program.muslExecutable,
            program.muslSharedLibrary, program.id, arguments)
        return new CrossLibcResults(glibcResult, muslResult)
    }

    @Override
    synchronized void close() {
        if (!closed.compareAndSet(false, true)) {
            return
        }
        stopQuietly(muslContainer)
        stopQuietly(glibcContainer)
        stopQuietly(buildContainer)
        transferDirectory.toFile().deleteDir()
    }

    private CompiledProgram compile(String classpathResource, String compiler,
                                    List<String> compilerArguments,
                                    String sharedLibraryResource) {
        requireStarted()
        String id = UUID.randomUUID().toString()
        String extension = classpathResource.endsWith('.cpp') ? '.cpp' : '.c'
        Path hostSource = transferDirectory.resolve("${id}${extension}")
        copyResource(classpathResource, hostSource)

        Path hostSharedLibrarySource = null
        if (sharedLibraryResource != null) {
            hostSharedLibrarySource = transferDirectory.resolve("${id}-module.c")
            copyResource(sharedLibraryResource, hostSharedLibrarySource)
        }

        String containerDirectory = "/tmp/musl-build-env-tests/${id}"
        String containerSource = "${containerDirectory}/sample${extension}"
        String containerMuslExecutable = "${containerDirectory}/sample-musl"
        String containerGlibcExecutable = "${containerDirectory}/sample-glibc"
        String containerSharedLibrarySource = "${containerDirectory}/module.c"
        String containerMuslSharedLibrary = "${containerDirectory}/module-musl.so"
        String containerGlibcSharedLibrary = "${containerDirectory}/module-glibc.so"

        assertSuccess(
            "create build directory for ${classpathResource}",
            buildContainer.execInContainer('mkdir', '-p', containerDirectory)
        )
        buildContainer.copyFileToContainer(
            MountableFile.forHostPath(hostSource), containerSource)

        Path hostMuslSharedLibrary = null
        Path hostGlibcSharedLibrary = null
        if (hostSharedLibrarySource != null) {
            buildContainer.copyFileToContainer(
                MountableFile.forHostPath(hostSharedLibrarySource),
                containerSharedLibrarySource)
            assertSuccess(
                "compile ${sharedLibraryResource}",
                buildContainer.execInContainer(
                    'musl-clang', containerSharedLibrarySource,
                    '-shared', '-fPIC', '-Wl,-soname,musl-test-module.so',
                    '-o', containerMuslSharedLibrary)
            )
            assertSuccess(
                "prepare the glibc shared library for ${sharedLibraryResource}",
                buildContainer.execInContainer(
                    'sh', '-ceu', glibcLibraryPatchScript(
                        containerMuslSharedLibrary,
                        containerGlibcSharedLibrary))
            )

            hostMuslSharedLibrary = transferDirectory.resolve("${id}-musl.so")
            hostGlibcSharedLibrary = transferDirectory.resolve("${id}-glibc.so")
            buildContainer.copyFileFromContainer(
                containerMuslSharedLibrary, hostMuslSharedLibrary.toString())
            buildContainer.copyFileFromContainer(
                containerGlibcSharedLibrary, hostGlibcSharedLibrary.toString())
        }

        List<String> command = [compiler, containerSource]
        command.addAll(compilerArguments)
        command.addAll(['-o', containerMuslExecutable])
        assertSuccess(
            "compile ${classpathResource}",
            buildContainer.execInContainer(command as String[])
        )

        assertSuccess(
            "prepare the glibc executable for ${classpathResource}",
            buildContainer.execInContainer(
                'sh', '-ceu', glibcPatchScript(
                    containerMuslExecutable, containerGlibcExecutable))
        )

        Path hostMuslExecutable = transferDirectory.resolve("${id}-musl")
        Path hostGlibcExecutable = transferDirectory.resolve("${id}-glibc")
        buildContainer.copyFileFromContainer(
            containerMuslExecutable, hostMuslExecutable.toString())
        buildContainer.copyFileFromContainer(
            containerGlibcExecutable, hostGlibcExecutable.toString())

        return new CompiledProgram(
            id, hostMuslExecutable, hostGlibcExecutable,
            hostMuslSharedLibrary, hostGlibcSharedLibrary, classpathResource)
    }

    private Container.ExecResult run(GenericContainer<?> runtimeContainer,
                                     Path executable, Path sharedLibrary,
                                     String id,
                                     List<String> arguments) {
        String runtimePath = "/tmp/musl-build-env-test-${id}"
        runtimeContainer.copyFileToContainer(
            MountableFile.forHostPath(executable), runtimePath)
        assertSuccess(
            "make ${runtimePath} executable",
            runtimeContainer.execInContainer('chmod', '0755', runtimePath)
        )
        List<String> command = [runtimePath]
        if (sharedLibrary != null) {
            String runtimeSharedLibraryPath = "${runtimePath}-module.so"
            runtimeContainer.copyFileToContainer(
                MountableFile.forHostPath(sharedLibrary),
                runtimeSharedLibraryPath)
            command.add(runtimeSharedLibraryPath)
        }
        command.addAll(arguments)
        return runtimeContainer.execInContainer(command as String[])
    }

    private static String glibcLibraryPatchScript(String source,
                                                  String destination) {
        return """
            cp '${source}' '${destination}'
            musl_libc=\$(patchelf --print-needed '${destination}' | sed -n '/^libc\\.musl/{p;q;}')
            if test -n "\${musl_libc}"; then
                patchelf --replace-needed "\${musl_libc}" libc.so.6 '${destination}'
            fi
            patchelf --add-needed libpthread.so.0 --add-needed libdl.so.2 '${destination}'
        """.stripIndent()
    }

    private String glibcPatchScript(String source, String destination) {
        String interpreter
        switch (architecture) {
            case 'x86_64':
                interpreter = '/lib64/ld-linux-x86-64.so.2'
                break
            case 'aarch64':
                interpreter = '/lib/ld-linux-aarch64.so.1'
                break
            default:
                throw new IllegalStateException("Unsupported architecture: ${architecture}")
        }

        return """
            cp '${source}' '${destination}'
            musl_libc=\$(patchelf --print-needed '${destination}' | sed -n '/^libc\\.musl/{p;q;}')
            test -n "\${musl_libc}"
            patchelf --set-interpreter '${interpreter}' \\
                --replace-needed "\${musl_libc}" libc.so.6 '${destination}'
            patchelf --add-needed libpthread.so.0 --add-needed libdl.so.2 '${destination}'
        """.stripIndent()
    }

    private static CrossLibcTestHarness fromSystemProperties() {
        CrossLibcTestHarness harness = new CrossLibcTestHarness(
            requiredProperty('buildEnvImage'),
            requiredProperty('glibcImage'),
            requiredProperty('muslImage'))
        Runtime.runtime.addShutdownHook(new Thread({ harness.close() }))
        return harness
    }

    private static String requiredProperty(String name) {
        String value = System.getProperty(name)
        if (!value) {
            throw new IllegalArgumentException("Missing system property: ${name}")
        }
        return value
    }

    private static GenericContainer<?> longRunningContainer(String image) {
        return new GenericContainer<>(DockerImageName.parse(image))
            .withCommand('sh', '-c', 'while :; do sleep 3600; done')
    }

    private static GenericContainer<?> runtimeContainer(String image) {
        GenericContainer<?> container = longRunningContainer(image)
        return container.withCreateContainerCmdModifier(
            new Consumer<CreateContainerCmd>() {
                @Override
                void accept(CreateContainerCmd command) {
                    // x86_64 MSan disables ASLR with personality(). Docker's
                    // default seccomp profile blocks that startup call.
                    command.hostConfig.withSecurityOpts(['seccomp=unconfined'])
                }
            })
    }

    private static void copyResource(String classpathResource, Path destination) {
        URL resource = CrossLibcTestHarness.classLoader.getResource(classpathResource)
        if (resource == null) {
            throw new IllegalArgumentException(
                "Classpath resource does not exist: ${classpathResource}")
        }
        resource.openStream().withCloseable { InputStream input ->
            Files.copy(input, destination, StandardCopyOption.REPLACE_EXISTING)
        }
    }

    private static void validateArchitecture(String architecture) {
        if (!(architecture in ['x86_64', 'aarch64'])) {
            throw new IllegalStateException(
                "The test harness supports x86_64 and aarch64, got: ${architecture}")
        }
    }

    private void requireStarted() {
        if (!started.get()) {
            throw new IllegalStateException('The cross-libc harness has not been started')
        }
    }

    private static String successfulOutput(String action,
                                           Container.ExecResult result) {
        assertSuccess(action, result)
        return result.stdout
    }

    private static void assertSuccess(String action, Container.ExecResult result) {
        if (result.exitCode != 0) {
            throw new IllegalStateException(
                "Failed to ${action} (exit ${result.exitCode})\n" +
                    "stdout:\n${result.stdout}\n" +
                    "stderr:\n${result.stderr}")
        }
    }

    private static void stopQuietly(GenericContainer<?> container) {
        if (container == null) {
            return
        }
        try {
            container.stop()
        } catch (Throwable ignored) {
            // Preserve the original startup/test failure.
        }
    }
}

@CompileStatic
final class CompiledProgram {
    final String id
    final Path muslExecutable
    final Path glibcExecutable
    final Path muslSharedLibrary
    final Path glibcSharedLibrary
    final String sourceResource

    CompiledProgram(String id, Path muslExecutable, Path glibcExecutable,
                    Path muslSharedLibrary, Path glibcSharedLibrary,
                    String sourceResource) {
        this.id = id
        this.muslExecutable = muslExecutable
        this.glibcExecutable = glibcExecutable
        this.muslSharedLibrary = muslSharedLibrary
        this.glibcSharedLibrary = glibcSharedLibrary
        this.sourceResource = sourceResource
    }
}

@CompileStatic
final class CrossLibcResults {
    final Container.ExecResult glibc
    final Container.ExecResult musl

    CrossLibcResults(Container.ExecResult glibc, Container.ExecResult musl) {
        this.glibc = glibc
        this.musl = musl
    }

    void assertSuccess() {
        assertRuntimeSuccess('glibc', glibc)
        assertRuntimeSuccess('musl', musl)
    }

    private static void assertRuntimeSuccess(String libc,
                                             Container.ExecResult result) {
        if (result.exitCode != 0) {
            throw new AssertionError(
                "Program failed on ${libc} (exit ${result.exitCode})\n" +
                    "stdout:\n${result.stdout}\n" +
                    "stderr:\n${result.stderr}")
        }
    }
}
