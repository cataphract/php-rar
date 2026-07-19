package org.cataphract.musl

import spock.lang.Shared
import spock.lang.Specification

abstract class CrossLibcSpecification extends Specification {
    @Shared
    protected CrossLibcTestHarness harness = CrossLibcTestHarness.shared()

    def setupSpec() {
        harness.start()
    }
}
