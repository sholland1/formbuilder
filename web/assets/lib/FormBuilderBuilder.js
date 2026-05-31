import FormBuilderCore from './FormBuilderCore.js';

export default class FormBuilderBuilder extends FormBuilderCore {
    #fieldIndex;
    #builderFormObject;

    constructor(document, dateGetter, builderFormObject) {
        super(document, dateGetter);
        this.#builderFormObject = builderFormObject;
        this.#fieldIndex = 0;
    }

    buildBuilder(builderElements) {
        this.build(this.#builderFormObject.forms.header, builderElements.header);

        const initialDragElement = this._document.getElementById('builder-initial-drag-element');
        this.addDragEvents(initialDragElement);

        this._document.getElementById('add_field').addEventListener('mousedown', () =>
            builderElements.fields.appendChild(this.createEmptyFieldForm()));
        this._document.getElementById('toggle_sections').addEventListener('mousedown', () => {
            const detailNodes = builderElements.fields.querySelectorAll('details');
            const anyOpen = Array.from(detailNodes).some(d => d.open);
            detailNodes.forEach(d => d.open = !anyOpen);
        });
    }

    editForm(builderElements, formData) {
        const header = builderElements.header;
        header.querySelector('#id').value = formData.id;
        header.querySelector('#title').value = formData.title;
        if (formData.answer_structure) {
            header.querySelector('#answer_structure').value = formData.answer_structure;
        }

        this.createFieldsAndAddToContainer(builderElements.fields, formData);
    }

    createFieldsAndAddToContainer(fieldContainer, formData) {
        for (const fieldData of formData.fields) {
            const currentFieldForm = this.createEmptyFieldForm();
            fieldContainer.appendChild(currentFieldForm);

            for (const propName in fieldData) {
                if (propName === 'fields') {
                    const innerFieldContainer = currentFieldForm.querySelector(':scope .builder-group > .builder-group-fields');
                    this.createFieldsAndAddToContainer(innerFieldContainer, fieldData);
                    continue;
                }
                const element = currentFieldForm.querySelector(`#${propName}`);
                if (!element) {
                    console.log(`element was null when creating field '${propName}'`);
                    break;
                }
                if (propName === 'required') {
                    element.value = fieldData[propName] !== false ? 'Yes' : 'No';
                }
                else {
                    element.value = fieldData[propName];
                }

                // TODO: Maybe call event functions directly
                if (propName === 'id') {
                    // triggers event to set header to id
                    element.dispatchEvent(new Event('keyup', { bubbles: true }));
                }
                else if (propName === 'type') {
                    // triggers event to set field type
                    element.dispatchEvent(new Event('change', { bubbles: true }));
                }
            }
        }
    }

    getFormBuilderFieldElements(fieldContainerElement) {
        const fieldElements = fieldContainerElement
            .querySelectorAll(':scope > details > .builder-field-start');
        return Array.from(fieldElements,
            element => {
                const firstElements = this.getFormElements(element);
                const secondElements = this.getFormElements(element.nextSibling);
                return [...firstElements, ...secondElements];
            });
    }

    getGroupInnerElements(item) {
        return this.getFormBuilderFieldElements(item.children[2]);
    }

    async processGroup(formData, item, answerStructure) {
        let fieldFormData = [];
        for (const field of item.fields) {
            let innerFormData = {};
            await this.processElements(innerFormData, field, answerStructure);
            fieldFormData.push(innerFormData);
        }

        switch (answerStructure) {
        case 'nested':
            formData.fields = fieldFormData;
            break;
        case 'flat':
            for (const id in fieldFormData) {
                formData[`${item.id}.${id}`] = fieldFormData[id];
            }
            break;
        case 'basic':
            formData.fields = fieldFormData;
            break;
        }
    }

    async getFormBuilderData(builderElements, answerStructure) {
        // Input is assumed valid
        const headerElements = this.getFormElements(builderElements.header);
        const fieldElements = this.getFormBuilderFieldElements(builderElements.fields);
        const formItems = [
            ...headerElements,
            {
                id: 'fields',
                type: 'group',
                fields: fieldElements
            },
        ];
        // TODO: use actual FormData object to upload data and files
        let formData = {};
        await this.processElements(formData, formItems, answerStructure);

        return formData;
    }

    addDragEvents(element) {
        element.addEventListener('dragenter', e => {
            e.stopPropagation();
            this._document.querySelectorAll('.dragging-over')
                .forEach(el => el.classList.remove('dragging-over'));
            e.currentTarget.classList.add('dragging-over');
        });
        element.addEventListener('dragleave', e => {
            if (!e.currentTarget.contains(e.relatedTarget)) {
                e.currentTarget.classList.remove('dragging-over');
            }
        });
        element.addEventListener('dragover', e => e.preventDefault());
        element.addEventListener('drop', e => {
            e.preventDefault();
            e.stopPropagation();
            e.currentTarget.classList.remove('dragging-over');
            const result = Number(e.dataTransfer.getData('text/plain'));
            e.currentTarget.after(this._document.getElementById(`field_${result}`));
        });
    }

    createEmptyFieldForm() {
        // Creating field elements
        const fieldGrip = this.element('div', { draggable: true, class: 'draggable', style: 'display:inline' }, '⣿⣿');
        const fieldId = this.element('span', { id: `field_id_${this.#fieldIndex}` }, `Field ${this.#fieldIndex}`);
        const fieldRemoveBtn = this.element('button', { id: `remove_field_${this.#fieldIndex}`, type: 'button' }, '🗑️');
        const fieldSummary = this.element('summary', {},
            fieldId,
            this.element('div', { class: 'builder-details-buttons' },
                fieldGrip, fieldRemoveBtn,
            ));
        const fieldStart = this.element('div', { id: `field_start_${this.#fieldIndex}`, class: 'builder-field-start' });
        const fieldRest = this.element('div', { id: `field_type_${this.#fieldIndex}` });
        const fieldForm = this.element('details', { id: `field_${this.#fieldIndex}` },
            fieldSummary, fieldStart, fieldRest);

        // Set up dragging
        const indexCopy = this.#fieldIndex;
        fieldGrip.addEventListener('dragstart', e => {
            requestAnimationFrame(() => this._document.body.classList.add('dragging'));
            e.dataTransfer.setData('text/plain', indexCopy);
        });
        fieldGrip.addEventListener('dragend', () => this._document.body.classList.remove('dragging'));
        this.addDragEvents(fieldForm);

        // Add empty form for field
        this.build(this.#builderFormObject.forms.field_start, fieldStart);

        // Set up dropdown to change field type
        // TODO: validate at least one field inside group
        // TODO: carry over label and other fields
        fieldForm
            .querySelector('#type')
            .addEventListener('change', e => {
                const fieldEntryFormData = this.#builderFormObject.forms[`field_type_${e.currentTarget.value}`];
                this.build(fieldEntryFormData, fieldRest);
                if (e.currentTarget.value !== 'group') return;

                const builderGroupFields = fieldRest.querySelector('.builder-group');
                builderGroupFields.remove();
                const groupTitle = builderGroupFields.children[0].innerText;
                builderGroupFields.children[0].remove();
                builderGroupFields.classList.replace('builder-group', 'builder-group-fields');

                const addFieldButton = this.element('button', { type: 'button' }, '+ Add field');
                addFieldButton.addEventListener('mousedown', () =>
                    builderGroupFields.appendChild(this.createEmptyFieldForm()));

                const toggleButton = this.element('button', { type: 'button' }, 'Expand/Collapse all');

                const nonDetailsStructure = this.element('div', { class: 'builder-item builder-group', style: 'padding-left:12px' },
                    this.element('label', { class: 'builder-label' }, groupTitle),
                    this.element('div', { class: 'builder-details-buttons' }, addFieldButton, toggleButton),
                    builderGroupFields);
                fieldRest.append(nonDetailsStructure);

                const placeholderField = builderGroupFields.getElementsByClassName('builder-field-container')[0];
                placeholderField.style.height = '8px';
                this.addDragEvents(placeholderField);

                toggleButton.addEventListener('mousedown', () => {
                    const detailNodes = nonDetailsStructure.querySelectorAll('#builder-fields details');
                    const anyOpen = Array.from(detailNodes).some(d => d.open);
                    detailNodes.forEach(d => d.open = !anyOpen);
                });
            });

        // Set up field id in details to match user entered id
        fieldForm.querySelector('input#id').addEventListener('keyup', e =>
            fieldId.innerText = e.currentTarget.value || `Field ${indexCopy}`);

        // Set up Shift+Tab to focus previous element
        const focusable = fieldForm.querySelector('input, select, textarea');
        focusable?.addEventListener('keydown', e => {
            if (e.shiftKey && e.key === 'Tab') {
                e.preventDefault();
                e.currentTarget.parentElement.parentElement.parentElement.previousElementSibling.focus();
            }
        });

        // Set up Tab to focus and open next element
        fieldForm.addEventListener('keydown', e => {
            if (e.key !== 'Tab' || e.shiftKey) return;
            if (!e.currentTarget.matches('summary')) return;

            e.preventDefault();
            e.currentTarget.open = true;

            const previous = e.currentTarget.previousElementSibling;
            if (previous && previous.tagName.toLowerCase() === 'details') previous.open = false;

            const fieldFocusable = e.currentTarget.querySelectorAll('input, select, textarea');
            if (fieldFocusable.length > 0) fieldFocusable[0].focus();
        });

        // Set up removal button
        fieldRemoveBtn.addEventListener('click', () => fieldForm.remove());

        this.#fieldIndex += 1;
        return fieldForm;
    }

    clear() {
        this.#fieldIndex = 0;
    }
}
