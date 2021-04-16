/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

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