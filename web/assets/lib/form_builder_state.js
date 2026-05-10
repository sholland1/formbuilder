import builderFormObject from '../builder.json' with { type: 'json' };
import FormBuilderBuilder from './FormBuilderBuilder.js';

export class FormBuilderState {
    constructor(doc, builderFormObject) {
        this.document = doc;
        this.builderFormObject = builderFormObject;
        this.builderElements = {
            title: doc.getElementById('builder-title'),
            header: doc.getElementById('builder-header'),
            fields: doc.getElementById('builder-fields'),
        };
        this.builder = new FormBuilderBuilder(doc, () => Temporal.Now.plainDateTimeISO(), builderFormObject);
        this.systemPromptText = null;
    }

    editForm(formData) {
        this.builder.editForm(this.builderElements, formData);
        this.setVisibility('builder');
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
        const builderFields = this.builderElements.fields;
        const placeholder = builderFields.firstElementChild;
        builderFields.replaceChildren(placeholder);

        this.builder.clear();
        this.builder.build(this.builderFormObject.forms.header, this.builderElements.header);

        this.document.querySelector('input#id').focus();
    }
}

export const formBuilderState = new FormBuilderState(document, builderFormObject);
