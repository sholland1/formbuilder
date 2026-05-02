import builderFormObject from '../builder.json' with { type: 'json' };
import FormBuilder from './FormBuilder.js';

export class FormBuilderState {
    constructor(doc) {
        this.document = doc;
        this.builderFormObject = builderFormObject;
        this.forms = builderFormObject.forms;
        this.builder = new FormBuilder(doc, () => Temporal.Now.plainDateTimeISO());
        this.systemPromptText = null;
        this.fieldIndex = 0;
        this.editFormHandler = null;
    }

    setEditFormHandler(handler) {
        this.editFormHandler = handler;
    }

    editForm(data) {
        if (this.editFormHandler) {
            this.editFormHandler(data);
        }
    }

    setVisibility(sectionId) {
        const sectionIds = ['intro', 'builder', 'genai'];
        for (const id of sectionIds) {
            this.document.getElementById(id).hidden = sectionId !== id;
        }

        let selector;
        if (sectionId === 'builder') {
            selector = 'input#id';
        } else if (sectionId === 'genai') {
            const apiKey = this.document.getElementById('api-key').value;
            selector = apiKey ? 'textarea#prompt' : 'input#api-key';
        }

        if (selector) {
            requestAnimationFrame(() => this.document.querySelector(selector).focus());
        }
    }

    clearForm() {
        this.fieldIndex = 0;
        const builderFields = this.document.getElementById('builder-fields');
        const placeholder = builderFields.firstElementChild;
        builderFields.replaceChildren(placeholder);

        const builderHeader = this.document.getElementById('builder-header');
        this.builder.build(this.forms.header, builderHeader);

        this.document.querySelector('input#id').focus();
    }
}

export const formBuilderState = new FormBuilderState(document);
