package org.cataphract.musl

import groovy.transform.CompileStatic
import groovy.transform.TupleConstructor
import org.testcontainers.containers.Container
import org.testcontainers.containers.GenericContainer
import org.testcontainers.utility.DockerImageName
import org.testcontainers.utility.MountableFile

import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardCopyOption

@CompileStatic
final class CrossLibcTestHarness implements Closeable {
    private final GenericContainer<?> buildContainer
    private final GenericContainer<?> glibcContainer
    private final GenericContainer<?> muslContainer
    private final Path transferDirectory

    private boolean closed
    private String architecture

    CrossLibcTestHarness(GenericContainer<?> buildContainer,
                        GenericContainer<?> glibcContainer,
                        GenericContainer<?> muslContainer) {
        this.buildContainer = buildContainer
        this.glibcContainer = glibcContainer
        this.muslContainer = muslContainer
        this.transferDirectory = Files.createTempDirectory('musl-build-env-tests-')
    }

    static GenericContainer<?> buildEnvironmentContainer() {
        return longRunningContainer(requiredProperty('buildEnvImage'))
    }

    static GenericContainer<?> glibcRuntimeContainer() {
        return longRunningContainer(requiredProperty('glibcImage'))
    }

    static GenericContainer<?> muslRuntimeContainer() {
        return longRunningContainer(requiredProperty('muslImage'))
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
        Container.ExecResult glibcResult = run(
            glibcContainer, program.glibcExecutable,
            program.glibcSharedLibrary, program.id, arguments)
        Container.ExecResult muslResult = run(
            muslContainer, program.muslExecutable,
            program.muslSharedLibrary, program.id, arguments)
        return new CrossLibcResults(glibcResult, muslResult)
    }

    Container.ExecResult buildEnvironmentCommand(List<String> command) {
        return buildContainer.execInContainer(command as String[])
    }

    String buildEnvironmentOutput(String action, List<String> command) {
        return successfulOutput(action, buildEnvironmentCommand(command))
    }

    void copyResourceToBuildEnvironment(String resource, String destination) {
        Path hostSource = transferDirectory.resolve("resource-${UUID.randomUUID()}")
        copyResource(resource, hostSource)
        buildContainer.copyFileToContainer(
            MountableFile.forHostPath(hostSource), destination)
    }

    @Override
    synchronized void close() {
        if (closed) {
            return
        }
        closed = true
        transferDirectory.toFile().deleteDir()
    }

    private CompiledProgram compile(String classpathResource, String compiler,
                                    List<String> compilerArguments,
                                    String sharedLibraryResource) {
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
        List<String> muslSharedLibraryDependencies = null
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
            muslSharedLibraryDependencies = neededLibraries(
                containerMuslSharedLibrary, sharedLibraryResource)
            assertSuccess(
                "prepare the glibc shared library for ${sharedLibraryResource}",
                buildContainer.execInContainer(
                    'cp', containerMuslSharedLibrary,
                    containerGlibcSharedLibrary)
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
        List<String> muslExecutableDependencies = neededLibraries(
            containerMuslExecutable, classpathResource)

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
            id, containerMuslExecutable,
            hostMuslExecutable, hostGlibcExecutable,
            hostMuslSharedLibrary, hostGlibcSharedLibrary,
            muslExecutableDependencies, muslSharedLibraryDependencies,
            classpathResource)
    }

    private List<String> neededLibraries(String path, String resource) {
        return successfulOutput(
            "read dependencies for ${resource}",
            buildContainer.execInContainer('patchelf', '--print-needed', path)
        ).readLines()
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

    private String glibcPatchScript(String source, String destination) {
        if (architecture == null) {
            architecture = successfulOutput(
                'detect the build image architecture',
                buildContainer.execInContainer('uname', '-m')
            ).trim()
            validateArchitecture(architecture)
        }

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
            patchelf --set-interpreter '${interpreter}' '${destination}'
        """.stripIndent()
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
}

@CompileStatic
@TupleConstructor
final class CompiledProgram {
    final String id
    final String buildContainerExecutable
    final Path muslExecutable
    final Path glibcExecutable
    final Path muslSharedLibrary
    final Path glibcSharedLibrary
    final List<String> muslExecutableDependencies
    final List<String> muslSharedLibraryDependencies
    final String sourceResource
}

@CompileStatic
@TupleConstructor
final class CrossLibcResults {
    final Container.ExecResult glibc
    final Container.ExecResult musl

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
