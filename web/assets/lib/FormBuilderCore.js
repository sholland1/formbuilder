export default class FormBuilderCore {
    constructor(document, dateGetter) {
        this._document = document;
        this._getDate = dateGetter;
    }

    // Creates a DOM element
    element(tagName, attrs, ...innerElements) {
        const element = this._document.createElement(tagName);
        for (const key in attrs) {
            if (attrs[key]) element.setAttribute(key, attrs[key]);
        }
        for (const elem of innerElements) {
            if (typeof elem === 'string') {
                element.appendChild(this._document.createTextNode(elem));
            }
            else if (typeof elem === 'object') {
                element.appendChild(elem);
            }
        }
        return element;
    }

    // Creates a group of DOM elements based on the field type
    _createComponent(field, currentDateStr) {
        if (field.type === 'timestamp' || field.type === 'guid') {
            return this.element('div', {id: field.id, class: 'builder-item builder-' + field.type });
        }

        let inputElements;
        if (field.type === 'text') {
            inputElements = [
                this.element('input', {
                    id: field.id, type: 'text',
                    pattern: field.pattern,
                    placeholder: field.placeholder ?? '',
                    maxlength: String(field.maxlength),
                    required: field.required !== false,
                })
            ];
        }
        else if (field.type === 'multitext') {
            inputElements = [
                this.element('input', {
                    id: field.id, type: 'text',
                    pattern: field.pattern,
                    placeholder: field.placeholder ?? '',
                    required: field.min > 0,
                })
            ];
        }
        else if (field.type === 'number') {
            inputElements = [
                this.element('input', {
                    id: field.id, type: 'number', required: field.required !== false,
                    min: String(field.min), max: String(field.max), step: String(field.step)
                }),
            ];
        }
        else if (field.type === 'select') {
            inputElements = [
                this.element('select', { id: field.id, required: field.required !== false },
                    this.element('option', {}, ''),
                    ...field.options.map(o => this.element('option', {}, o)))
            ];
        }
        else if (field.type === 'multiselect') {
            let i = 0;
            inputElements =
                field.options.map(o =>
                    this.element('label', { for: field.id + i },
                        this.element('input', { id: field.id + i++, type: 'checkbox', value: o }),
                        o));
        }
        else if (field.type === 'date') {
            const minDate = field.min === 'today' ? currentDateStr : field.min;
            const maxDate = field.max === 'today' ? currentDateStr : field.max;
            inputElements = [
                this.element('input', {
                    id: field.id, type: field.type,
                    min: minDate, max: maxDate,
                    required: field.required !== false,
                })
            ];
        }
        else if (field.type === 'counter') {
            let inputElement = this.element('input', {
                id: field.id, type: 'number',
                step: '1', value: '0', disabled: true
            });
            const plusButton = this.element('button', { type: 'button', class: 'builder-button-plus' }, '+');
            const minusButton = this.element('button', { type: 'button', class: 'builder-button-minus' }, '-');
            const clearButton = this.element('button', { type: 'button', class: 'builder-button-clear' }, 'Clear');
            plusButton.addEventListener('click', () => inputElement.value++);
            minusButton.addEventListener('click',
                () => inputElement.value = Math.max(inputElement.value-1, 0));
            clearButton.addEventListener('click', () => inputElement.value = 0);

            inputElements = [ inputElement, plusButton, minusButton, clearButton ];
        }
        else if (field.type === 'color') {
            inputElements = [ this.element('input', { id: field.id, type: field.type }) ];
        }
        else if (field.type === 'bool') {
            const required = field.required !== false;
            let items = required ? [] : [''];
            items.push('Yes', 'No');
            inputElements = [
                this.element('select', { id: field.id, required },
                    ...items.map(o => this.element('option', {}, o)))
            ];
        }
        else if (field.type === 'timer') {
            function formatDuration(ms) {
                if (typeof ms !== 'number' || isNaN(ms) || ms < 0) {
                    return '00:00:00:00';
                }

                let cs = Math.trunc(ms / 10);

                const h = Math.trunc(cs / 360000) % 100;
                cs %= 360000;
                const m = Math.trunc(cs / 6000);
                cs %= 6000;
                const s = Math.trunc(cs / 100);
                const c = cs % 100;

                return (
                    (h < 10 ? '0' : '') + h + ':' +
                    (m < 10 ? '0' : '') + m + ':' +
                    (s < 10 ? '0' : '') + s + ':' +
                    (c < 10 ? '0' : '') + c
                );
            }

            let canvas = this.element('canvas',
                { id: field.id, width: 160, height: 50, style: 'border: solid' });
            const ctx = canvas.getContext('2d', { alpha: true });

            function draw(elapsed) {
                ctx.clearRect(0, 0, canvas.width, canvas.height);

                ctx.save();
                ctx.font = 'bold 24px monospace';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillStyle = '#000000';

                const text = formatDuration(elapsed);
                ctx.fillText(text, canvas.width / 2, canvas.height / 2);
                ctx.restore();
            }

            const startButton = this.element('button', { type: 'button', class: 'builder-button-start-stop' }, 'Start');
            const resetButton = this.element('button', { type: 'button', class: 'builder-button-reset' }, 'Reset');
            let hiddenDurationValue = this.element('input', {
                id: field.id, type: 'number',
                hidden: true, value: '0'
            });

            let startTime = 0;
            let accumulatedTime = 0;
            let isRunning = false;
            let rafId = null;

            function updateTimer() {
                if (!isRunning) return;

                const elapsed = performance.now() - startTime + accumulatedTime;

                draw(elapsed);
                rafId = requestAnimationFrame(updateTimer);
            }

            function startTimer() {
                if (isRunning) return;
                isRunning = true;
                startTime = performance.now();
                updateTimer();
            }

            function pauseTimer() {
                if (!isRunning) return;
                isRunning = false;
                cancelAnimationFrame(rafId);
                accumulatedTime += performance.now() - startTime;
            }

            function resetTimer() {
                pauseTimer();
                accumulatedTime = 0;
            }

            startButton.addEventListener('click', () => {
                if (resetButton.disabled) {
                    startButton.textContent = 'Start';
                    resetButton.disabled = false;
                    pauseTimer();
                    draw(accumulatedTime);
                    hiddenDurationValue.value = Math.trunc(accumulatedTime / 10);
                    return;
                }

                startButton.textContent = 'Stop';
                resetButton.disabled = true;
                startTimer();
            });

            resetButton.addEventListener('click', () => {
                resetTimer();
                draw(0);
                hiddenDurationValue.value = 0;
            });

            draw(0);

            inputElements = [
                hiddenDurationValue,
                this.element('div', {}, canvas),
                startButton, resetButton,
            ];
        }
        else if (field.type === 'file') {
            inputElements = [
                this.element('input', {
                    id: field.id,
                    type: 'file',
                    required: field.min > 0,
                    multiple: (field.min || 1) > 1 || (field.max || 1) > 1,
                    accept: field.fileexts,
                }),
            ];
        }
        else if (field.type === 'signature') {
            let canvas = this.element('canvas',
                { id: field.id, width: 600, height: 300, style: 'border: solid' });
            const ctx = canvas.getContext('2d');
            let dragging = false;
            let last_position = null;

            let clearButton = this.element('button', { type: 'button' }, 'Clear');
            clearButton.addEventListener('click', () =>
                ctx.clearRect(0, 0, canvas.width, canvas.height));

            function drawLine(ctx, last_pos, curr_pos) {
                ctx.beginPath();
                ctx.moveTo(last_pos.x, last_pos.y);
                ctx.lineTo(curr_pos.x, curr_pos.y);
                ctx.lineWidth = 4;
                ctx.lineCap = 'round';
                ctx.stroke();
            }

            canvas.addEventListener('mousedown', e => {
                dragging = true;
                last_position = {x: e.offsetX, y: e.offsetY};
                drawLine(ctx, last_position, last_position);
            });
            canvas.addEventListener('mouseup', () => dragging = false);
            canvas.addEventListener('mouseleave', () => dragging = false);
            canvas.addEventListener('mousemove', e => {
                if (!dragging) return;

                let current_position = {x: e.offsetX, y: e.offsetY};
                drawLine(ctx, last_position, current_position);
                last_position = current_position;
            });

            inputElements = [ canvas, this.element('div', {}, clearButton)];
        }
        else if (field.type === 'group') {
            inputElements = field.fields.flatMap(f => this._createComponent(f, currentDateStr));
        }
        else {
            console.log('Unsupported element type: ' + field.type);
            inputElements = ['Unsupported element type: ' + field.type];
        }

        return this.element('div', { class: 'builder-item builder-' + field.type },
            this.element('label', { for: field.id, class: 'builder-label' }, field.label),
            this.element('div', { class: 'builder-field-container' }, ...inputElements));
    }

    // Builds a form composed of components
    build(formObject, formDiv) {
        formDiv.innerHTML = '';

        if (!formObject) {
            console.log(`formObject was null, id: ${formDiv.id}`);
            return;
        }
        if (formObject.title) {
            formDiv.appendChild(
                this.element('h1', { id: 'builder-title' }, formObject.title));
        }

        const currentDateStr = this._getDate().toPlainDate().toString(); //format date like YYYY-MM-DD
        for (const f of formObject.fields) {
            formDiv.appendChild(this._createComponent(f, currentDateStr));
        }

        const inputs = formDiv.getElementsByTagName('input');
        if (inputs.length > 0) {
            inputs[0].focus();
        }
    }

    // Gets a file from the user for the 'file' field type
    _readFile(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();

            reader.onload = () => resolve(reader.result);
            reader.onerror = reject;

            reader.readAsDataURL(file);
        });
    }

    getFormElements(element) {
        let typedElements = [];
        const items = element.querySelectorAll(':scope > .builder-item');
        for (const item of items) {
            const type = Array.from(item.classList)
                .find(c => c !== 'builder-item')
                .replace('builder-', '');
            if (type === 'timestamp' || type === 'guid') {
                typedElements.push({id: item.id, type: type});
                continue;
            }
            else if (type === 'group') {
                const groupId = item.querySelector('label[for]').getAttribute('for');
                const innerElements = this.getGroupInnerElements(item);
                typedElements.push({id: groupId, type: type, fields: innerElements});
                continue;
            }

            const inputs = item.querySelectorAll('[id]');

            if (type === 'multiselect') {
                const actualId = item.querySelector('label[for]').getAttribute('for');
                typedElements.push({id: actualId, type: type, element: inputs.entries().map(x => x[1]) });
                continue;
            }

            if (type === 'rating') continue;

            typedElements.push({id: inputs[0].id, type: type, element: inputs[0]});
        }

        return typedElements;
    }

    async processElements(formData, items, answerStructure) {
        for (const element of items) {
            await this.processElement(formData, element, answerStructure);
        }
    }

    async processElement(formData, item, answerStructure) {
        if (item.type === 'timestamp') {
            formData[item.id] = this._getDate();
        }
        else if (item.type === 'guid') {
            formData[item.id] = crypto.randomUUID();
        }
        else if (item.type === 'group') {
            await this.processGroup(formData, item, answerStructure);
        }
        else if (item.type === 'multiselect') {
            formData[item.id] = item.element
                .filter(input => input.checked)
                .map(input => input.value)
                .toArray();
        }
        else if (item.type === 'counter') {
            formData[item.id] = Number(item.element.value);
        }
        else if (item.type === 'color') {
            formData[item.id] = item.element.value;
        }
        else if (item.type === 'bool') {
            if (item.element.value === 'No') {
                formData[item.id] = false;
            }
        }
        else if (item.type === 'number') {
            if (item.element.required || item.element.value) {
                formData[item.id] = Number(item.element.value);
            }
        }
        else if (item.type === 'select') {
            if (item.element.required || item.element.value) {
                formData[item.id] = isNaN(item.element.value) ? item.element.value : Number(item.element.value);
            }
        }
        else if (item.type === 'multitext') {
            if (item.element.required || item.element.value) {
                formData[item.id] = item.element.value.split(',').map(s => s.trim());
            }
        }
        else if (item.type === 'timer') {
            const durationInMs = Number(item.element.value) * 10;
            formData[item.id] = Temporal.Duration.from({milliseconds: durationInMs});
        }
        else if (item.type === 'file') {
            const files = Array.from(item.element.files);
            formData[item.id] = await Promise.all(
                files.map(async f => ({
                    name: f.name,
                    type: f.type,
                    size: f.size,
                    data: await this._readFile(f),
                })));
        }
        else if (item.type === 'signature') {
            formData[item.id] = {
                name: `${item.id}_signature.png`,
                type: 'image/png',
                data: item.element.toDataURL('image/png', 1),
            };
        }
        else {
            if (item.element?.required || item.element?.value) {
                formData[item.id] = item.element.value;
            }
        }
    }

    // Overridden by adapter classes
    getGroupInnerElements() {
        throw new Error('getGroupInnerElements not implemented');
    }

    // Overridden by adapter classes
    async processGroup() {
        throw new Error('processGroup not implemented');
    }
}
