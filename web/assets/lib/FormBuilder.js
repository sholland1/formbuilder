import FormBuilderCore from './FormBuilderCore.js';

export default class FormBuilder extends FormBuilderCore {
    constructor(document, dateGetter) {
        super(document, dateGetter);
    }

    getGroupInnerElements(item) {
        return this.getFormElements(item.children[1]);
    }

    async processGroup(formData, item, answerStructure) {
        let innerFormData = {};
        for (const field of item.fields) {
            await this.processElement(innerFormData, field, answerStructure);
        }

        switch (answerStructure) {
        case 'nested':
            formData[item.id] = innerFormData;
            break;
        case 'flat':
            for (const id in innerFormData) {
                formData[`${item.id}.${id}`] = innerFormData[id];
            }
            break;
        case 'basic':
            for (const id in innerFormData) {
                formData[id] = innerFormData[id];
            }
            break;
        }
    }

    // Collects the user entered data from a form
    async getFormData(element, answerStructure) {
        const formItems = this.getFormElements(element);
        // TODO: use actual FormData object to upload data and files
        let formData = {};
        await this.processElements(formData, formItems, answerStructure);

        return formData;
    }
}
