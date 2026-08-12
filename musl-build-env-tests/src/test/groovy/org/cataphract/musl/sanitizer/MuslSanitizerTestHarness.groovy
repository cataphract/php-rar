package org.cataphract.musl.sanitizer

import com.github.dockerjava.api.command.CreateContainerCmd
import groovy.transform.CompileStatic
import groovy.transform.MapConstructor
import org.testcontainers.containers.Container
import org.testcontainers.containers.GenericContainer
import org.testcontainers.utility.DockerImageName
import org.testcontainers.utility.MountableFile

import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.util.function.Consumer

@CompileStatic
final class MuslSanitizerTestHarness implements Closeable {
    private static final String ASAN_LIBRARY_DIRECTORY = '/usr/asan/lib'
    private static final String MSAN_LIBRARY_DIRECTORY = '/usr/msan/lib'
    private static final String DEFAULT_LIBRARY_DIRECTORY = '/usr/lib'

    private final GenericContainer<?> buildContainer
    private final GenericContainer<?> muslContainer
    private final Path transferDirectory

    private boolean closed

    MuslSanitizerTestHarness(GenericContainer<?> buildContainer,
                             GenericContainer<?> muslContainer) {
        this.buildContainer = buildContainer
        this.muslContainer = muslContainer
        this.transferDirectory = Files.createTempDirectory(
            'musl-sanitizer-tests-')
    }

    static GenericContainer<?> buildEnvironmentContainer() {
        return longRunningContainer(requiredProperty('buildEnvImage'))
    }

    static GenericContainer<?> muslRuntimeContainer() {
        return sanitizerRuntimeContainer(requiredProperty('muslImage'))
    }

    MuslSanitizerProgram compileC(
        String classpathResource, List<String> compilerArguments = []) {
        return compile(
            resource: classpathResource, compilerArguments: compilerArguments)
    }

    MuslSanitizerProgram compileCpp(
        String classpathResource, List<String> compilerArguments = []) {
        return compile(
            resource: classpathResource, compiler: 'musl-clang++',
            compilerArguments: compilerArguments)
    }

    MuslSanitizerProgram compileCWithSharedLibrary(
        String classpathResource, String sharedLibraryResource,
        List<String> compilerArguments = [],
        List<String> sharedLibraryCompilerArguments = []) {
        return compile(
            resource: classpathResource, compilerArguments: compilerArguments,
            sharedLibraryResource: sharedLibraryResource,
            sharedLibraryCompilerArguments: sharedLibraryCompilerArguments)
    }

    /**
     * Runs a command in the build container. Specs use this to stage toolchain
     * state that the link line is expected to pick up on its own, such as the
     * downstream compatibility archive or /etc/musl-clang.conf, and to inspect
     * links that {@link #compileC} cannot express.
     */
    Container.ExecResult buildEnvironmentCommand(List<String> command) {
        return buildContainer.execInContainer(command as String[])
    }

    /**
     * As {@link #buildEnvironmentCommand}, but rejects a failing command
     * instead of returning it.
     */
    String buildEnvironmentOutput(String action, List<String> command) {
        return successfulOutput(action, buildEnvironmentCommand(command))
    }

    void copyResourceToBuildEnvironment(
        String classpathResource, String containerPath) {
        Path hostCopy = transferDirectory.resolve(
            "${UUID.randomUUID()}-${classpathResource.split('/').last()}")
        copyResource(classpathResource, hostCopy)
        buildContainer.copyFileToContainer(
            MountableFile.forHostPath(hostCopy), containerPath)
    }

    Container.ExecResult run(
        MuslSanitizerProgram program, List<String> arguments = [],
        Map<String, String> environment = [:]) {
        String runtimePath = "/tmp/musl-sanitizer-test-${program.id}"
        muslContainer.copyFileToContainer(
            MountableFile.forHostPath(program.executable), runtimePath)
        assertSuccess(
            "make ${runtimePath} executable",
            muslContainer.execInContainer('chmod', '0755', runtimePath))

        Map<String, String> runtimeEnvironment =
            new LinkedHashMap<>(environment)
        if (!program.runtimeLibraries.isEmpty()) {
            String runtimeLibraryDirectory = "${runtimePath}-libs"
            assertSuccess(
                "create ${runtimeLibraryDirectory}",
                muslContainer.execInContainer(
                    'mkdir', '-p', runtimeLibraryDirectory))
            program.runtimeLibraries.each {
                String libraryName, Path hostLibrary ->
                muslContainer.copyFileToContainer(
                    MountableFile.forHostPath(hostLibrary),
                    "${runtimeLibraryDirectory}/${libraryName}")
            }
            runtimeEnvironment.putIfAbsent(
                'LD_LIBRARY_PATH', runtimeLibraryDirectory)
        }

        List<String> command = ['env']
        runtimeEnvironment.toSorted().each { String name, String value ->
            command.add("${name}=${value}".toString())
        }
        command.add(runtimePath)
        if (program.sharedLibrary != null) {
            String runtimeSharedLibraryPath = "${runtimePath}-module.so"
            muslContainer.copyFileToContainer(
                MountableFile.forHostPath(program.sharedLibrary),
                runtimeSharedLibraryPath)
            command.add(runtimeSharedLibraryPath)
        }
        command.addAll(arguments)
        return muslContainer.execInContainer(command as String[])
    }

    @Override
    synchronized void close() {
        if (closed) {
            return
        }
        closed = true
        transferDirectory.toFile().deleteDir()
    }

    private MuslSanitizerProgram compile(Map<String, ?> options) {
        String classpathResource = (String) options.resource
        String compiler = (String) (options.compiler ?: 'musl-clang')
        List<String> compilerArguments =
            (List<String>) (options.compilerArguments ?: [])
        String sharedLibraryResource = (String) options.sharedLibraryResource
        List<String> sharedLibraryCompilerArguments =
            (List<String>) (options.sharedLibraryCompilerArguments ?: [])

        String id = UUID.randomUUID().toString()
        String extension = classpathResource.endsWith('.cpp') ? '.cpp' : '.c'
        Path hostSource = transferDirectory.resolve("${id}${extension}")
        copyResource(classpathResource, hostSource)

        String containerDirectory = "/tmp/musl-sanitizer-tests/${id}"
        String containerSource = "${containerDirectory}/sample${extension}"
        String containerExecutable = "${containerDirectory}/sample"
        String libraryDirectory = selectLibraryDirectory(compilerArguments)
        assertSuccess(
            "create build directory for ${classpathResource}",
            buildContainer.execInContainer(
                'mkdir', '-p', containerDirectory))
        buildContainer.copyFileToContainer(
            MountableFile.forHostPath(hostSource), containerSource)

        Path hostSharedLibrary = null
        List<String> sharedLibraryDependencies = null
        if (sharedLibraryResource != null) {
            Path hostSharedLibrarySource =
                transferDirectory.resolve("${id}-module.c")
            copyResource(sharedLibraryResource, hostSharedLibrarySource)
            String containerSharedLibrarySource =
                "${containerDirectory}/module.c"
            String containerSharedLibrary =
                "${containerDirectory}/module.so"
            buildContainer.copyFileToContainer(
                MountableFile.forHostPath(hostSharedLibrarySource),
                containerSharedLibrarySource)

            List<String> sharedCommand = [
                compiler, containerSharedLibrarySource,
                '-shared', '-fPIC', '-Wl,-soname,musl-sanitizer-module.so',
                "-L${libraryDirectory}".toString(),
                '-Wl,-t',
            ]
            sharedCommand.addAll(sharedLibraryCompilerArguments)
            sharedCommand.addAll(['-o', containerSharedLibrary])
            Container.ExecResult sharedLink =
                buildContainer.execInContainer(sharedCommand as String[])
            assertSuccess(
                "compile ${sharedLibraryResource}", sharedLink)
            assertLinkTrace(
                sharedLibraryResource, libraryDirectory,
                "${sharedLink.stdout}\n${sharedLink.stderr}".toString())
            sharedLibraryDependencies = neededLibraries(
                containerSharedLibrary, sharedLibraryResource)

            hostSharedLibrary = transferDirectory.resolve("${id}-module.so")
            buildContainer.copyFileFromContainer(
                containerSharedLibrary, hostSharedLibrary.toString())
        }

        List<String> command = [
            compiler, containerSource,
            "-L${libraryDirectory}".toString(),
            '-Wl,-t',
        ]
        command.addAll(compilerArguments)
        command.addAll(['-o', containerExecutable])
        Container.ExecResult executableLink =
            buildContainer.execInContainer(command as String[])
        assertSuccess("compile ${classpathResource}", executableLink)
        assertLinkTrace(
            classpathResource, libraryDirectory,
            "${executableLink.stdout}\n${executableLink.stderr}".toString())

        List<String> allDependencies = neededLibraries(
            containerExecutable, classpathResource)
        if (sharedLibraryDependencies != null) {
            allDependencies.addAll(sharedLibraryDependencies)
        }
        Map<String, Path> runtimeLibraries = copyRuntimeLibraries(
            libraryDirectory, allDependencies, id,
            classpathResource)
        String symbolTable = successfulOutput(
            "read symbols for ${classpathResource}",
            buildContainer.execInContainer(
                'llvm-readelf', '--symbols', containerExecutable))

        Path hostExecutable = transferDirectory.resolve("${id}-sample")
        buildContainer.copyFileFromContainer(
            containerExecutable, hostExecutable.toString())
        return new MuslSanitizerProgram(
            id: id,
            executable: hostExecutable,
            sharedLibrary: hostSharedLibrary,
            runtimeLibraries: runtimeLibraries,
            symbolTable: symbolTable)
    }

    private static String selectLibraryDirectory(
        List<String> compilerArguments) {
        Set<String> sanitizers = new LinkedHashSet<>()
        compilerArguments.each { String argument ->
            if (argument.startsWith('-fsanitize=')) {
                Collections.addAll(
                    sanitizers,
                    argument.substring('-fsanitize='.length()).split(','))
            }
        }
        if (sanitizers.contains('memory')) {
            return MSAN_LIBRARY_DIRECTORY
        }
        if (sanitizers.contains('address')) {
            return ASAN_LIBRARY_DIRECTORY
        }
        return DEFAULT_LIBRARY_DIRECTORY
    }

    private Map<String, Path> copyRuntimeLibraries(
        String libraryDirectory, Collection<String> initialDependencies,
        String id, String resource) {
        Map<String, Path> libraries = new LinkedHashMap<>()
        Set<String> pending = new LinkedHashSet<>(initialDependencies)
        while (!pending.isEmpty()) {
            String libraryName = pending.iterator().next()
            pending.remove(libraryName)
            if (libraries.containsKey(libraryName)) {
                continue
            }

            String candidate = "${libraryDirectory}/${libraryName}"
            Container.ExecResult exists = buildContainer.execInContainer(
                'test', '-e', candidate)
            if (exists.exitCode != 0) {
                continue
            }

            String resolved = successfulOutput(
                "resolve ${libraryName} for ${resource}",
                buildContainer.execInContainer(
                    'readlink', '-f', candidate)).trim()
            Path hostLibrary =
                transferDirectory.resolve("${id}-${libraryName}")
            buildContainer.copyFileFromContainer(
                resolved, hostLibrary.toString())
            libraries.put(libraryName, hostLibrary)
            pending.addAll(neededLibraries(resolved, libraryName))
        }
        return libraries
    }

    private static void assertLinkTrace(
        String resource, String libraryDirectory, String linkerTrace) {
        if (libraryDirectory == DEFAULT_LIBRARY_DIRECTORY) {
            boolean linkedNativeLibc = linkerTrace.readLines().any {
                String line ->
                line.startsWith('/lib/ld-musl-') && line.endsWith('.so.1')
            }
            if (!linkedNativeLibc) {
                throw new IllegalStateException(
                    "Sanitizer sample ${resource} did not link native musl " +
                    "through ${DEFAULT_LIBRARY_DIRECTORY}")
            }
            return
        }
        if (linkerTrace.contains('libglibc_compat.a')) {
            throw new IllegalStateException(
                "Sanitizer sample ${resource} linked glibc_compat.a")
        }
        // The directory's libc.so is a linker script, which ld.lld does not
        // report; the DSO it selects is the traceable proof it was used.
        String nativeLibc = "${libraryDirectory}/libc-native.so"
        if (!linkerTrace.readLines().contains(nativeLibc)) {
            throw new IllegalStateException(
                "Sanitizer sample ${resource} did not link ${nativeLibc}")
        }
    }

    private List<String> neededLibraries(String path, String resource) {
        return successfulOutput(
            "read dependencies for ${resource}",
            buildContainer.execInContainer(
                'patchelf', '--print-needed', path)
        ).readLines()
    }

    private static String requiredProperty(String name) {
        String value = System.getProperty(name)
        if (!value) {
            throw new IllegalArgumentException(
                "Missing system property: ${name}")
        }
        return value
    }

    private static GenericContainer<?> longRunningContainer(String image) {
        return new GenericContainer<>(DockerImageName.parse(image))
            .withCommand('sh', '-c', 'while :; do sleep 3600; done')
    }

    private static GenericContainer<?> sanitizerRuntimeContainer(String image) {
        GenericContainer<?> container = longRunningContainer(image)
        return container.withCreateContainerCmdModifier(
            new Consumer<CreateContainerCmd>() {
                @Override
                void accept(CreateContainerCmd command) {
                    // x86_64 MSan disables ASLR with personality().
                    command.hostConfig.withSecurityOpts(['seccomp=unconfined'])
                }
            })
    }

    private static void copyResource(String resource, Path destination) {
        URL source = MuslSanitizerTestHarness.classLoader.getResource(resource)
        if (source == null) {
            throw new IllegalArgumentException(
                "Classpath resource does not exist: ${resource}")
        }
        source.openStream().withCloseable { InputStream input ->
            Files.copy(
                input, destination, StandardCopyOption.REPLACE_EXISTING)
        }
    }

    private static String successfulOutput(
        String action, Container.ExecResult result) {
        assertSuccess(action, result)
        return result.stdout
    }

    private static void assertSuccess(
        String action, Container.ExecResult result) {
        if (result.exitCode != 0) {
            throw new IllegalStateException(
                "Failed to ${action} (exit ${result.exitCode})\n" +
                    "stdout:\n${result.stdout}\n" +
                    "stderr:\n${result.stderr}")
        }
    }
}

@CompileStatic
@MapConstructor
final class MuslSanitizerProgram {
    final String id
    final Path executable
    final Path sharedLibrary
    final Map<String, Path> runtimeLibraries
    final String symbolTable

    boolean hasWeakFunctionSymbol(String functionName) {
        return hasFunctionSymbolAttribute(functionName, 'WEAK')
    }

    boolean hasHiddenFunctionSymbol(String functionName) {
        return hasFunctionSymbolAttribute(functionName, 'HIDDEN')
    }

    private boolean hasFunctionSymbolAttribute(
        String functionName, String attribute) {
        return symbolTable.readLines().any { String line ->
            line.contains('FUNC') && line.contains(attribute) &&
                line.endsWith(" ${functionName}")
        }
    }
}
