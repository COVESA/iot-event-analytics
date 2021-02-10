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
        const features = Object.keys(ioteaTypeFeatures[type]);

        // Set features and add description title
        replaceOptionItems(
            '#ioteaEventFeature',
            features,
            feature => `${feature}&nbsp;[${ioteaTypeFeatures[type][feature].unit}]`,
            feature => feature,
            (itemElem, feature) => itemElem.title = ioteaTypeFeatures[type][feature].description
        );

        // Select first feature
        setValue('#ioteaEventFeature', features[0]);

        // Update Message
        updateJsonMessage('feature', features[0]);
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

            // Clear message
            updateJsonMessage('type', '');
            updateJsonMessage('feature', '');

            // Filter types for those, who have features
            const types = Object.keys(ioteaTypeFeatures).filter(type => Object.keys(ioteaTypeFeatures[type]).length > 0);

            if (types.length === 0) {
                return;
            }

            const type = types[0];

            replaceOptionItems('#ioteaEventType', types);
            updateJsonMessage('type', type);
            // Update IoTeaEventFeature
            onIoTeaEventTypeChange(type);
        });
}

function submitMessage() {
    let msg = getJsonMessage();

    if (msg !== undefined) {
        if (getValue('#messageType') === 'ioteaevent' && getValue('#ioteaUpdateTimestampMs')) {
            updateJsonMessage('whenMs', Date.now());
            msg = getJsonMessage();
        }

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