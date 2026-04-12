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
        if (item.type === 'timestamp' || item.type === 'guid') {
            return this.element('div', {id: item.id, class: 'builder-item builder-' + item.type })
        }

        let inputElements;
        if (item.type === 'text') {
            inputElements = [ this.element('input', { id: item.id, type: 'text', pattern: item.pattern, placeholder: item.placeholder ?? '', maxlength: String(item.maxlength), required: item.required !== false }) ];
        }
        else if (item.type === 'multitext') {
            inputElements = [ this.element('input', { id: item.id, type: 'text', pattern: item.pattern, placeholder: item.placeholder ?? '', required: item.min > 0 }) ];
        }
        else if (item.type === 'number') {
            inputElements = [
                this.element('input', {
                    id: item.id, type: 'number', required: item.required !== false,
                    min: String(item.min), max: String(item.max), step: String(item.step)
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
                    this.element('input', { id: item.id + i, type: 'checkbox', value: o, required: item.min > 0 }),
                    this.element('label', { for: item.id + i++ }, o),
                ]);
        }
        else if (item.type === 'date') {
            const minDate = item.min === 'today' ? currentDateStr : item.min;
            const maxDate = item.max === 'today' ? currentDateStr : item.max;
            inputElements = [ this.element('input', { id: item.id, type: item.type, min: minDate, max: maxDate, required: item.required !== false }) ];
        }
        else if (item.type === 'counter') {
            let inputElement = this.element('input', { id: item.id, type: 'number', step: '1', value: '0', disabled: true });
            let plusButton = this.element('button', { type: 'button', class: 'builder-button-plus' }, '+');
            let minusButton = this.element('button', { type: 'button', class: 'builder-button-minus' }, '-');
            let clearButton = this.element('button', { type: 'button', class: 'builder-button-clear' }, 'Clear');
            plusButton.addEventListener('click', () => inputElement.value++);
            minusButton.addEventListener('click', () => inputElement.value = Math.max(inputElement.value-1, 0));
            clearButton.addEventListener('click', () => inputElement.value = 0);

            inputElements = [ inputElement, plusButton, minusButton, clearButton ];
        }
        else if (item.type === 'color') {
            inputElements = [ this.element('input', { id: item.id, type: item.type }) ];
        }
        else if (item.type === 'bool') {
            const required = item.required !== false;
            let items = required ? [] : [''];
            items.push('Yes', 'No');
            inputElements = [
                this.element('select', { id: item.id, required },
                    ...items.map(o => this.element('option', {}, o)))
            ];
        }
        else if (item.type === 'timer') {
            function formatDuration(ms) {
                if (typeof ms !== 'number' || isNaN(ms) || ms < 0) {
                    return "00:00:00:00";
                }
                // Convert to centiseconds (1/100th of a second)
                let totalCentiseconds = Math.trunc(ms / 10);

                const hours = Math.trunc(totalCentiseconds / (3600 * 100));
                totalCentiseconds %= 3600 * 100;

                const minutes = Math.trunc(totalCentiseconds / (60 * 100));
                totalCentiseconds %= 60 * 100;

                const seconds = Math.trunc(totalCentiseconds / 100);
                const centiseconds = totalCentiseconds % 100;

                // Pad with leading zeros
                const hh = String(hours).padStart(2, '0');
                const mm = String(minutes).padStart(2, '0');
                const ss = String(seconds).padStart(2, '0');
                const tt = String(centiseconds).padStart(2, '0');

                return `${hh}:${mm}:${ss}:${tt}`;
            }

            let startButton = this.element('button', { type: 'button', class: 'builder-button-start-stop' }, 'Start');
            let resetButton = this.element('button', { type: 'button', class: 'builder-button-reset' }, 'Reset');
            let currentDuration = 0;
            let hiddenDurationValue = this.element('input', { id: item.id, type: 'number', hidden: true, value: '0' });
            let durationDisplay = this.#document.createTextNode(formatDuration(currentDuration));

            startButton.addEventListener('mousedown', e => {
                if (resetButton.disabled) {
                    startButton.textContent = 'Start';
                    resetButton.disabled = false;
                    return;
                }
                startButton.textContent = 'Stop';
                resetButton.disabled = true;

                const currentTime = performance.now();
                function addSec(d) {
                    if (!resetButton.disabled) {
                        currentDuration = d;
                        hiddenDurationValue.value = Math.trunc(currentDuration / 10);
                        return;
                    }
                    requestAnimationFrame(() => {
                        const newDuration = performance.now() - currentTime  + currentDuration;
                        durationDisplay.data = formatDuration(newDuration);
                        addSec(newDuration);
                    });
                }
                addSec(currentDuration);
            });

            resetButton.addEventListener('mousedown', e => {
                currentDuration = 0;
                durationDisplay.data = formatDuration(currentDuration);
                hiddenDurationValue.value = currentDuration;
            });

            inputElements = [
                hiddenDurationValue,
                this.element('div', {}, durationDisplay),
                startButton, resetButton,
            ];
        }
        else if (item.type === 'file') {
            inputElements = [
                this.element('input', {
                    id: item.id,
                    type: 'file',
                    required: item.min > 0,
                    multiple: (item.min || 1) > 1 || (item.max || 1) > 1,
                    accept: item.fileexts,
                }),
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

    #readFile(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();

            reader.onload = () => resolve(reader.result);
            reader.onerror = reject;

            reader.readAsDataURL(file);
        });
    }

    async getFormData(id) {
        let element = id ? this.#document.getElementById(id) : this.#document;
        const items = element.getElementsByClassName('builder-item');
        // TODO: use actual FormData object to upload data and files
        let formData = {};
        for (const item of items) {
            const type = Array.from(item.classList).find(c => c !== 'builder-item').replace('builder-', '');
            if (type === 'timestamp') {
                formData[item.id] = this.#getDate();
                continue;
            }
            else if (type === 'guid') {
                formData[item.id] = crypto.randomUUID();
                continue;
            }

            let inputs = Array.from(item.getElementsByTagName('*')).filter(e => e.id)

            if (type === 'multiselect') {
                const actual_id = Array.from(item.getElementsByTagName('label'))
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
            else if (type === 'timer') {
                const durationInMs = Number(input.value) * 10;
                formData[input.id] = Temporal.Duration.from({milliseconds: durationInMs});
            }
            else if (type === 'file') {
                let files = Array.from(input.files);
                let readFile = this.#readFile;
                formData[input.id] = await Promise.all(
                    files.map(async f => ({
                        name: f.name,
                        type: f.type,
                        size: f.size,
                        data: await readFile(f),
                    })));
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
