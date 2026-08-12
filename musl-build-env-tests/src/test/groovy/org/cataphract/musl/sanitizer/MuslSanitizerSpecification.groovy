package org.cataphract.musl.sanitizer

import org.testcontainers.containers.GenericContainer
import org.testcontainers.spock.Testcontainers
import spock.lang.AutoCleanup
import spock.lang.Shared
import spock.lang.Specification

@Testcontainers
abstract class MuslSanitizerSpecification extends Specification {
    @Shared
    protected GenericContainer<?> buildContainer =
        MuslSanitizerTestHarness.buildEnvironmentContainer()

    @Shared
    protected GenericContainer<?> muslContainer =
        MuslSanitizerTestHarness.muslRuntimeContainer()

    @Shared
    @AutoCleanup
    protected MuslSanitizerTestHarness harness =
        new MuslSanitizerTestHarness(buildContainer, muslContainer)
}
