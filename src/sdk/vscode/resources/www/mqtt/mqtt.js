const vscode = acquireVsCodeApi();

let ioteaValueSelectValues = {};
let ioteaTypeFeatures = {};

function setMessageType(type) {
    for (let type_ of ['none', 'ioteaevent']) {
        if (type_ === type) {
            show(`.${type_}`);
        } else {
            hide(`.${type_}`);
        }
    }

    switch (type) {
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
    catch (err) {
        return undefined;
    }
}

function updateTopicNs(topicNs) {
    setInnerHtml('.topicNs', `${topicNs}&nbsp;`);
}

function createJsonMessage(type) {
    switch (type) {
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

function onIoTeaEventValueUpdate(value, tryJson = false) {
    if (tryJson) {
        try {
            value = JSON.parse(value);
        }
        catch (err) { }
    }

    updateJsonMessage('value', value);
}

function onIoTeaEventFeatureChange(feature) {
    const type = getValue('#ioteaEventType');

    updateJsonMessage('feature', feature);

    const metaFeature = ioteaTypeFeatures[type][feature];

    // Hide all value inputs
    hide('.iotea-event-value');

    let value = metaFeature.encoding.default;

    if (metaFeature.encoding.encoder === 'category') {
        show('.value-enum');

        let selectedValue = value;
        ioteaValueSelectValues = {};

        if (selectedValue === undefined) {
            selectedValue = metaFeature.encoding.enum[0];
        }

        replaceOptionItems('.value-enum-select', metaFeature.encoding.enum, value => `${value}`, value => {
            const someUid = uuidv4();

            ioteaValueSelectValues[someUid] = value;

            if (value === selectedValue) {
                selectedValue = someUid;
            }

            return someUid;
        });

        setValue('.value-enum-select', selectedValue);
    } else if (metaFeature.encoding.type === 'number') {
        if (metaFeature.encoding.encoder === 'minmax' || metaFeature.encoding.encoder === 'through') {
            let min = 0;
            let max = 1;

            if (metaFeature.encoding.encoder === 'minmax') {
                min = metaFeature.encoding.min;
                max = metaFeature.encoding.max;
            }

            if (value === undefined) {
                value = min + Math.round((max - min) / 2);
            }

            show('.value-range');
            setAttribute('.value-range', 'min', min);
            setAttribute('.value-range', 'max', max);
            setValue('.value-range', value);
        } else {
            show('.value-number');
            setValue('.value-number', value || 0);
        }
    } else if (metaFeature.encoding.type === 'string') {
        show('.value-string');
        setValue('.value-string', value || '');
    } else if (metaFeature.encoding.type === 'object' || metaFeature.encoding.type === 'array' || metaFeature.encoding.type === 'any') {
        show('.value-object');

        if (metaFeature.encoding.type === 'array') {
            setValue('.value-object', JSON.stringify(Array.isArray(value) ? value : [], null, 2));
        } else {
            setValue('.value-object', JSON.stringify(value || {}, null, 2));
        }
    }
}

function onIoTeaEventTypeChange(type) {
    if (Object.prototype.hasOwnProperty.call(ioteaTypeFeatures, type)) {
        const features = Object.keys(ioteaTypeFeatures[type]).sort((a, b) => a.localeCompare(b));

        const vssPathTranslator = new VssPathTranslator(
            VSS_PATH_SEPARATOR,
            VSS_PATH_REPLACER
        );

        function formatFeatureDisplayName(feature) {
            const vssPath = vssPathTranslator.ioteaFeature2KuksaPartialVssPath(feature);

            if (ioteaTypeFeatures[type][feature].unit === undefined) {
                return vssPath;
            }

            return `${vssPath}&nbsp;[${ioteaTypeFeatures[type][feature].unit}]`;
        }

        // Set features and add description title
        replaceOptionItems(
            '#ioteaEventFeature',
            features,
            formatFeatureDisplayName,
            feature => feature,
            (itemElem, feature) => itemElem.title = ioteaTypeFeatures[type][feature].description
        );

        // Select first feature
        setValue('#ioteaEventFeature', features[0]);
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
            const types = Object.keys(ioteaTypeFeatures).filter(type => Object.keys(ioteaTypeFeatures[type]).length > 0).sort((a, b) => a.localeCompare(b));

            if (types.length === 0) {
                return;
            }

            const type = types[0];

            replaceOptionItems('#ioteaEventType', types);
            updateJsonMessage('type', type);
            // Update IoT Event Analytics Feature
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

    vscode.postMessage({
        broker: getValue('#mqttEndpoint'),
        topic: getValue('#topicNs').trim() + getValue('#topic').trim(),
        // Be aware, that you need to convert \" to \\\", because the escaped quotation marks need to stay intact
        message: msg.replace(/\x22/g, '\\\x22')
    });
}

window.addEventListener('message', ev => {
    console.log(ev.data);

    show('#result');

    const data = ev.data;
    const elem = document.querySelector('#result');

    let vscodeColorClass = '--vscode-foreground';

    if (data) {
        elem.innerHTML = '&nbsp;Success';
    } else {
        elem.innerHTML = `&nbsp;${data.error}`;
        vscodeColorClass = '--vscode-errorForeground';
    }

    setStyle('#result', 'color', `var(${vscodeColorClass})`);

    setTimeout(() => { hide('#result'); }, 2500);
});