package org.cataphract.musl

import org.testcontainers.containers.GenericContainer
import org.testcontainers.spock.Testcontainers
import spock.lang.AutoCleanup
import spock.lang.Shared
import spock.lang.Specification

@Testcontainers
abstract class CrossLibcSpecification extends Specification {
    @Shared
    protected GenericContainer<?> buildContainer =
        CrossLibcTestHarness.buildEnvironmentContainer()

    @Shared
    protected GenericContainer<?> glibcContainer =
        CrossLibcTestHarness.glibcRuntimeContainer()

    @Shared
    protected GenericContainer<?> muslContainer =
        CrossLibcTestHarness.muslRuntimeContainer()

    @Shared
    @AutoCleanup
    protected CrossLibcTestHarness harness = new CrossLibcTestHarness(
        buildContainer, glibcContainer, muslContainer)
}
