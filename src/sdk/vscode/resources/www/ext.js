async function httpGetJson(url) {
    return fetch(url, { method: 'GET' }).then(response => response.json());
}

async function replaceOptionItems(selector, items, getDisplayName = i => i, getValue = i => i, onListItemElem = (itemElem, item) => {}) {
    const listElem = document.querySelector(selector);

    if (listElem === null) {
        return;
    }

    // Remove everything, which was there
    listElem.innerHTML = '';

    for (let item of items) {
        const itemElem = document.createElement('option');

        let value = getValue(item);

        if (value === undefined) {
            value = '';
        }

        itemElem.value = value;

        if (item === null) {
            itemElem.selected = true;
            itemElem.disabled = true;
        }

        itemElem.innerHTML = getDisplayName(item) || '';

        onListItemElem(itemElem, item);

        listElem.appendChild(itemElem);
    }
}

function setValue(selector, value) {
    __setProperty(selector, 'value', value);
}

function setInnerHtml(selector, html) {
    __setProperty(selector, 'innerHTML', html);
}

function __setProperty(selector, property, value) {
    const elems = document.querySelectorAll(selector);

    for (let i = 0; i < elems.length; ++i) {
        const elem = elems[i];
        elem[property] = value;

        if (elem.onchange) {
            // Trigger onchange listener
            var evt = document.createEvent('HTMLEvents');
            evt.initEvent('change', false, true);
            elem.dispatchEvent(evt);
        }
    }
}

function getValue(selector) {
    const elem = document.querySelector(selector);

    if (elem === null) {
        return;
    }

    if (elem.tagName.toUpperCase() === 'INPUT' && getAttribute(elem, 'type') === 'checkbox') {
        return elem.checked;
    }

    return elem.value;
}

function getAttribute(elem, attr, defaultValue) {
    if (!elem.hasAttribute(attr)) {
        if (defaultValue === undefined) {
            throw new Error(`Attribute ${attr} not found for elem of type ${elem.tagName} and no default value given`);
        }

        return defaultValue;
    }

    return elem.getAttribute(attr);
}

function setStyle(selector, property, value) {
    const elems = document.querySelectorAll(selector);

    for (let i = 0; i < elems.length; ++i) {
        const elem = elems[i];

        // snake-case to CamelCase
        property = property.replace(/(-[a-z])/ig, $1 => $1.toUpperCase().replace('-', ''));

        elem.style[property] = value;
    }
}

function addClass(selector, class_) {
    const elems = document.querySelectorAll(selector);

    for (let i = 0; i < elems.length; ++i) {
        const elem = elems[i];
        const classes = getAttribute(elem, 'class', '').split(' ');
        classes.push(class_);
        elem.setAttribute('class', Array.from(new Set(classes.filter(class_ => class_ !== ''))).join(' '));
    }
}

function removeClass(selector, class_) {
    const elems = document.querySelectorAll(selector);

    for (let i = 0; i < elems.length; ++i) {
        const elem = elems[i];
        const classes = getAttribute(elem, 'class', '').split(' ');
        elem.setAttribute('class', classes.filter(class__ => class__ !== '' && class__ !== class_).join(' '));
    }
}

function setAttribute(selector, attr, value) {
    document.querySelectorAll(selector).forEach(elem => {
        elem.setAttribute(attr, value);
    });
}

function show(selector) {
    removeClass(selector, 'hidden');
}

function hide(selector) {
    addClass(selector, 'hidden');
}

class VssPathTranslator {
    constructor(vssPathSeparator, vssPathReplacer) {
        this.vssPathSeparator = vssPathSeparator;
        this.vssPathReplacer = vssPathReplacer;
    }

    ioteaFeature2KuksaPartialVssPath(feature) {
        let kuksaPartialVssPath = feature;
        for (let replacement in this.vssPathReplacer) {
            kuksaPartialVssPath = this.replaceAllChars(kuksaPartialVssPath, this.vssPathReplacer[replacement], replacement);
        }
        return kuksaPartialVssPath;
    }

    replaceAllChars(input, searchChar, replaceChar) {
        const reservedRegexpChars = ['[', ']', '(', ')', '\\', '^', '$', '.', '|', '?', '*', '+', '{', '}'];
        if (reservedRegexpChars.indexOf(searchChar) === -1) {
            return input.replace(new RegExp(`${searchChar}`, 'g'), replaceChar);
        }
        return input.replace(new RegExp(`\\${searchChar}`, 'g'), replaceChar);
    }
}

function uuidv4() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
        const r = Math.random() * 16 | 0;
        return (c == 'x' ? r : (r & 0x3 | 0x8)).toString(16);
    });
}