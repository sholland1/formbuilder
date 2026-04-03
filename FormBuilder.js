export default class FormBuilder {
    #document;
    #getDate;

    constructor(document, dateGetter) {
        this.#document = document;
        this.#getDate = dateGetter;
    }

    element(tagName, attrs, ...innerElements) {
        let element = this.#document.createElement(tagName);
        for (const key in attrs) {
            if (attrs[key]) element.setAttribute(key, attrs[key]);
        }
        for (const elem of innerElements) {
            if (typeof elem === 'string') {
                element.appendChild(this.#document.createTextNode(elem));
            }
            else if (typeof elem === 'object') {
                element.appendChild(elem);
            }
        }
        return element;
    }

    #createComponent(item, currentDateStr) {
        if (item.type === 'timestamp') {
            return this.element('div', {id: item.id, class: 'builder-item builder-' + item.type })
        }

        let inputElements;
        if (item.type === 'text') {
            inputElements = [ this.element('input', { id: item.id, type: 'text', pattern: item.pattern, placeholder: item.placeholder ?? '', maxlength: item.maxlength, required: item.required !== false }) ];
        }
        else if (item.type === 'multitext') {
            inputElements = [ this.element('input', { id: item.id, type: 'text', pattern: item.pattern, placeholder: item.placeholder ?? '', required: item.required !== false }) ];
        }
        else if (item.type === 'number') {
            inputElements = [
                this.element('input', { id: item.id, type: item.type, pattern: '\\d*', min: item.min, max: item.max, step: item.step, required: item.required !== false,
                    oninput: `this.nextElementSibling.value = this.value;`
                }),
                this.element('input', {
                    id: item.id + '-slider', type: 'range', min: item.min, max: item.max, step: item.step, required: item.required !== false,
                    oninput: `this.previousElementSibling.value = this.value;`
                }),
            ];
        }
        else if (item.type === 'select') {
            inputElements = [
                this.element('select', { id: item.id, required: item.required !== false },
                    this.element('option', {}, ''),
                    ...item.options.map(o => this.element('option', {}, o)))
            ];
        }
        else if (item.type === 'multiselect') {
            let i = 0;
            inputElements =
                item.options.flatMap(o => [
                    this.element('input', { id: item.id + i, type: 'checkbox', value: o }),
                    this.element('label', { for: item.id + i++ }, o),
                ]);
        }
        else if (item.type === 'date') {
            let minDate = item.min === 'today' ? currentDateStr : item.min;
            let maxDate = item.max === 'today' ? currentDateStr : item.max;
            inputElements = [ this.element('input', { id: item.id, type: item.type, min: minDate, max: maxDate, required: item.required !== false }) ];
        }
        else if (item.type === 'counter') {
            inputElements = [
                this.element('input', { id: item.id, type: 'number', step: 1, value: '0', disabled: true }),
                this.element('button', { type: 'button', class: 'builder-button-plus', onclick: `let ee = this.parentElement.children[0];ee.value = (+ee.value)+1;` }, '+'),
                this.element('button', { type: 'button', class: 'builder-button-minus', onclick: `let ee = this.parentElement.children[0];ee.value = (+ee.value) === 0 ? 0 : (+ee.value)-1;` }, '-'),
                this.element('button', { type: 'button', class: 'builder-button-clear', onclick: `this.parentElement.children[0].value = 0;` }, 'Clear'),
            ];
        }
        else if (item.type === 'color') {
            inputElements = [ this.element('input', { id: item.id, type: item.type }) ];
        }
        else if (item.type === 'bool') {
            let required = item.required !== false;
            let items = required ? [] : [''];
            items.push('Yes', 'No');
            inputElements = [
                this.element('select', { id: item.id, required },
                    ...items.map(o => this.element('option', {}, o)))
            ];
        }
        else if (item.type === 'timer') {
            function formatDuration(duration) {
                // Round to centiseconds (hundredths of a second) for clean tt
                const rounded = duration.round({
                    smallestUnit: 'millisecond',   // or 'microsecond' for more precision
                    roundingIncrement: 10,         // 10 ms = 1 centisecond
                    roundingMode: 'halfExpand'     // standard rounding
                });

                // Get total seconds including fractional part
                const totalSeconds = rounded.total({ unit: 'second' });

                // Extract components
                const hours = Math.floor(totalSeconds / 3600);
                const minutes = Math.floor((totalSeconds % 3600) / 60);
                const seconds = Math.floor(totalSeconds % 60);

                // Hundredths of a second (tt)
                const fractional = totalSeconds % 1;           // e.g. 0.5678
                const hundredths = Math.round(fractional * 100); // 0–99

                // Zero-pad everything
                const hh = String(hours).padStart(2, '0');
                const mm = String(minutes).padStart(2, '0');
                const ss = String(seconds).padStart(2, '0');
                const tt = String(hundredths).padStart(2, '0');

                return `${hh}:${mm}:${ss}:${tt}`;
            }

            let startButton = this.element('button', { type: 'button', class: 'builder-button-start-stop' }, 'Start');
            let resetButton = this.element('button', { type: 'button', class: 'builder-button-reset' }, 'Reset');
            let currentDuration = new Temporal.Duration();
            startButton.addEventListener('click', e => {
                if (resetButton.disabled) {
                    startButton.innerText = 'Start';
                    resetButton.disabled = false;
                    return;
                }
                startButton.innerText = 'Stop';
                resetButton.disabled = true;

                let ee = e.target.parentElement.children[0].children[0];
                let currentTime = this.#getDate();
                let currentObject = this;
                function addSec(d) {
                    if (!resetButton.disabled) {
                        currentDuration = d;
                        return;
                    }
                    setTimeout(() => {
                        let newDuration = currentObject.#getDate().since(currentTime).add(currentDuration);
                        ee.innerText = formatDuration(newDuration);
                        addSec(newDuration);
                    }, 10);
                }
                addSec(currentDuration);
            });
            resetButton.addEventListener('click', e => {
                let ee = e.target.parentElement.children[0].children[0];
                currentDuration = new Temporal.Duration();
                ee.innerText = formatDuration(currentDuration);
            });

            inputElements = [
                this.element('div', { id: item.id },
                    this.element('span', {}, formatDuration(currentDuration))),
                startButton, resetButton,
            ];
        }
        else {
            inputElements = ['Unsupported element type: ' + item.type];
        }

        return this.element('div', { class: 'builder-item builder-' + item.type },
            this.element('label', { for: item.id, class: 'builder-question' }, item.question),
            this.element('div', {}, ...inputElements));
    }

    build(formObject, id) {
        const formDiv = this.#document.getElementById(id ?? formObject.id);
        formDiv.innerHTML = '';

        if (formObject.title) formDiv.appendChild(this.element('h1', { id: 'builder-title' }, formObject.title));

        const currentDateStr = this.#getDate().toPlainDate().toString(); //format date like YYYY-MM-DD
        for (const f of formObject.fields) {
            formDiv.appendChild(this.#createComponent(f, currentDateStr));
        }

        const inputs = formDiv.getElementsByTagName('input');
        if (inputs.length > 0)
            inputs[0].focus();
    }

    getFormData(id) {
        let element = id ? this.#document.getElementById(id) : this.#document;
        const items = element.getElementsByClassName('builder-item');
        let formData = {};
        for (const item of items) {
            let type = Array.from(item.classList).find(c => c !== 'builder-item').replace('builder-', '');
            if (type === 'timestamp') {
                formData[item.id] = this.#getDate();
                continue;
            }

            let inputs = Array.from(item.getElementsByTagName('*')).filter(e => e.id)

            if (type === 'multiselect') {
                let actual_id = Array.from(item.getElementsByTagName('label'))
                    .find(l => l.classList.contains('builder-question'))
                    .getAttribute('for');
                formData[actual_id] = inputs
                    .filter(input => input.checked)
                    .map(input => input.value);
                continue;
            }

            const input = inputs[0];
            if (type === 'counter') {
                formData[input.id] = Number(input.value);
            }
            else if (type === 'color') {
                formData[input.id] = input.value;
            }
            else if (type === 'bool') {
                if (input.value === 'No') {
                    formData[input.id] = false;
                }
            }
            else if (type === 'number') {
                if (input.required || input.value) {
                    formData[input.id] = Number(input.value);
                }
            }
            else if (type === 'select') {
                if (input.required || input.value) {
                    formData[input.id] = isNaN(input.value) ? input.value : Number(input.value);
                }
            }
            else if (type === 'multitext') {
                if (input.required || input.value) {
                    formData[input.id] = input.value.split(',').map(s => s.trim());
                }
            }
            else {
                if (input.required || input.value) {
                    formData[input.id] = input.value;
                }
            }
        }

        return formData;
    }
}
