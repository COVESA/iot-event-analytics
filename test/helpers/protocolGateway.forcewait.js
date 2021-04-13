function forceWaitOnPublish(pg) {
    // eslint-disable-next-line jasmine/no-unsafe-spy
    spyOn(pg, 'publish').and.callFake(function (topic, message, publishOptions) {
        return this.publish.and.originalFn.call(this, topic, message, publishOptions, true);
    });
}

function forceWaitOnSubscribe(pg) {
    // eslint-disable-next-line jasmine/no-unsafe-spy
    spyOn(pg, 'subscribe').and.callFake(function (topic, callback, subscribeOptions) {
        return this.subscribe.and.originalFn.call(this, topic, callback, subscribeOptions, true);
    });
}

function forceWaitOnSubscribeShared(pg) {
    // eslint-disable-next-line jasmine/no-unsafe-spy
    spyOn(pg, 'subscribeShared').and.callFake(function (topic, callback, subscribeOptions) {
        return this.subscribeShared.and.originalFn.call(this, topic, callback, subscribeOptions, true);
    });
}

module.exports = {
    forceWaitOnPublish,
    forceWaitOnSubscribe,
    forceWaitOnSubscribeShared
};