let ioteaTypeFeatures = {};

function setMessageType(type) {
    for (let type_ of [ 'none', 'ioteaevent' ]) {
        if (type_ === type) {
            show(`.${type_}`);
        } else {
            hide(`.${type_}`);
        }
    }

    switch(type) {
        case 'ioteaevent': {
            updateIoteaTypeFeatures();
            setValue('#topic', 'ingestion/events');
            break;
        }
        default: {
            setValue('#topic', '');
        }
    }

    setJsonMessage(createJsonMessage(type));
}

function setMessage(msg) {
    setValue('#content', msg);
}

function setJsonMessage(json) {
    setMessage(JSON.stringify(json, null, 2));
}

function updateJsonMessage(key, value) {
    const json = getJsonMessage();

    if (json === undefined) {
        return;
    }

    json[key] = value;

    setJsonMessage(json);
}

function getMessage() {
    return getValue('#content');
}

function getJsonMessage() {
    try {
        return JSON.parse(getMessage());
    }
    catch(err) {
        return undefined;
    }
}

function updateTopicNs(topicNs) {
    setInnerHtml('.topicNs', `${topicNs}&nbsp;`);
}

function createJsonMessage(type) {
    switch(type) {
        case 'none': {
            return {};
        }
        case 'ioteaevent': {
            return {
                "feature": "",
                "type": "",
                "subject": "test",
                "instance": "test",
                "whenMs": Date.now(),
                "value": ""
            };
        }
    }
}

function onIoTeaEventTypeChange(type) {
    if (Object.prototype.hasOwnProperty.call(ioteaTypeFeatures, type)) {
        replaceOptionItems('#ioteaEventFeature', [null, ...Object.keys(ioteaTypeFeatures[type])]);
    }
}

function updateIoteaTypeFeatures() {
    return httpGetJson(getValue('#ioteaMetadataApiEndpoint'))
        .then(types => {
            const typeFeatures = {};

            for (let type in types) {
                if (types[type].features === undefined) {
                    continue;
                }

                typeFeatures[type] = [];

                for (let feature in types[type].features) {
                    typeFeatures[type][feature] = types[type].features[feature];
                }
            }

            return typeFeatures;
        })
        .then(typeFeatures => {
            ioteaTypeFeatures = typeFeatures;
            updateJsonMessage('type', '');
            updateJsonMessage('feature', '');
            replaceOptionItems('#ioteaEventType', [null, ...Object.keys(ioteaTypeFeatures)]);
            replaceOptionItems('#ioteaEventFeature', []);
        });
}

function submitMessage() {
    let msg = getJsonMessage();

    if (msg !== undefined) {
        msg = JSON.stringify(msg);
    } else {
        msg = getMessage();
    }

    postMessage({
        broker: getValue('#mqttEndpoint'),
        topic: getValue('#topicNs').trim() + getValue('#topic').trim(),
        // Be aware, that you need to convert \" to \\\", because the escaped quotation marks need to stay intact
        message: msg.replace(/\x22/g, '\\\x22')
    });
}