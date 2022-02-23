'use strict';

// This mock provides a way to intercept renderer <> browser mojo messages for
// openTCPSocket(...) and openUDPSocket(...) eliminating the need for an actual
// browser.
// See https://wicg.github.io/direct-sockets/

async function loadChromiumResources() {
    await import ('/resources/chromium/mock-direct-sockets.js');
}

let mockDirectSocketsService = null;

async function createMockDirectSocketsService() {
    if (typeof DirectSocketsServiceTest === 'undefined') {
        await loadChromiumResources();
    }
    assert_implements(DirectSocketsServiceTest, 'DirectSocketsServiceTest is not loaded properly.');

    if (mockDirectSocketsService === null) {
        mockDirectSocketsService = new DirectSocketsServiceTest();
    } else {
        mockDirectSocketsService.reset();
    }
    mockDirectSocketsService.initialize();

    return mockDirectSocketsService;
}

function direct_sockets_test(func, description) {
    promise_test(async test => {
        const directSocketsServiceTest = await createMockDirectSocketsService();
        await func(test, mockDirectSocketsService);
    }, description);
}
