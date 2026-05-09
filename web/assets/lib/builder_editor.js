import { formBuilderState as app } from './form_builder_state.js';

const createElementCreator = (tagName) => (a, ...b) => app.builder.element(tagName, a, ...b);

const div = createElementCreator('div');
const summary = createElementCreator('summary');
const details = createElementCreator('details');
const span = createElementCreator('span');
const button = createElementCreator('button');

function addDragEvents(element, outerElement) {
    element.addEventListener('dragenter', e => e.currentTarget.classList.add('dragging-over'));
    element.addEventListener('dragleave', e => e.currentTarget.classList.remove('dragging-over'));
    element.addEventListener('dragover', e => e.preventDefault());
    element.addEventListener('drop', e => {
        e.preventDefault();
        e.currentTarget.classList.remove('dragging-over');
        const result = Number(e.dataTransfer.getData('text/plain'));
        outerElement.after(document.getElementById(`field_${result}`));
    });
}

function createEmptyFieldForm() {
    // Creating field elements
    const fieldGrip = div({ draggable: true, class: 'draggable', style: 'display:inline' }, '⣿⣿');
    const fieldId = span({ id: `field_id_${app.fieldIndex}` }, `Field ${app.fieldIndex}`);
    const fieldRemoveBtn = button({ id: `remove_field_${app.fieldIndex}`, type: 'button' }, '🗑️');
    const fieldSummary = summary({},
        fieldId,
        div({ class: 'builder-details-buttons' },
            fieldGrip, fieldRemoveBtn,
        ));
    const fieldStart = div({ id: `field_start_${app.fieldIndex}`, class: 'builder-field-start' });
    const fieldRest = div({ id: `field_type_${app.fieldIndex}` });
    const fieldForm = details({ id: `field_${app.fieldIndex}` },
        fieldSummary, fieldStart, fieldRest);

    // Set up dragging
    const indexCopy = app.fieldIndex;
    fieldGrip.addEventListener('dragstart', e => {
        requestAnimationFrame(() => document.body.classList.add('dragging'));
        e.dataTransfer.setData('text/plain', indexCopy);
    });
    fieldGrip.addEventListener('dragend', () => document.body.classList.remove('dragging'));
    addDragEvents(fieldSummary, fieldForm);

    // Add empty form for field
    app.builder.build(app.forms.field_start, fieldStart);

    // Set up dropdown to change field type
    fieldForm
        .querySelector('#type')
        .addEventListener('change', e =>
            app.builder.build(app.forms[`field_type_${e.currentTarget.value}`], fieldRest));

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

    app.fieldIndex += 1;
    return fieldForm;
}

const globalBuilder = {
    title: document.getElementById('builder-title'),
    header: document.getElementById('builder-header'),
    fields: document.getElementById('builder-fields'),
};

function editForm(formData) {
    const header = globalBuilder.header;
    header.querySelector('#id').value = formData.id;
    header.querySelector('#title').value = formData.title;
    if (formData.answer_structure) {
        header.querySelector('#answer_structure').value = formData.answer_structure;
    }

    for (const fieldData of formData.fields) {
        const currentFieldForm = createEmptyFieldForm();
        globalBuilder.fields.appendChild(currentFieldForm);

        for (const propName in fieldData) {
            const element = currentFieldForm.querySelector(`#${propName}`);
            if (!element) {
                console.log(`element was null when creating field '${propName}'`);
                break;
            }
            if (propName === 'required') {
                element.value = fieldData[propName] !== false ? 'Yes' : 'No';
            } else {
                element.value = fieldData[propName];
            }
            if (propName === 'id') {
                element.dispatchEvent(new Event('keyup', { bubbles: true }));
            } else if (propName === 'type') {
                element.dispatchEvent(new Event('change', { bubbles: true }));
            }
        }
    }
    app.setVisibility('builder');
}

app.setEditFormHandler(editForm);

globalBuilder.title.innerText = app.builderFormObject.title;
app.builder.build(app.forms.header, globalBuilder.header);

const initialDragElement = document.getElementById('builder-initial-drag-element');
addDragEvents(initialDragElement, initialDragElement);

document.getElementById('add_field').addEventListener('mousedown', () =>
    globalBuilder.fields.appendChild(createEmptyFieldForm()));
document.getElementById('toggle_sections').addEventListener('mousedown', () => {
    const detailNodes = globalBuilder.fields.querySelectorAll('details');
    const anyOpen = Array.from(detailNodes).some(d => d.open);
    detailNodes.forEach(d => d.open = !anyOpen);
});

function validateForm() {
    const form = document.getElementById(app.builderFormObject.id);

    // Open details tags of any invalid field forms
    form.querySelectorAll('[required]').forEach(el => {
        if (el.checkValidity()) return;
        const d = el.closest('details');
        if (d) d.open = true;
    });

    // Ensure all field ids are unique
    // TODO: Display error if duplicate ids found
    const ids = Array.from(form.querySelectorAll('#id')).map(e => e.value);
    if (ids.some((id, i) => id && ids.indexOf(id) !== i)) return false;

    // Check validity based on built in input attributes
    // Highlights inputs in red and navigates to first invalid one
    return form.reportValidity();
}

async function getFormData() {
    // Input is assumed valid
    const formData = await app.builder.getFormData(globalBuilder.header, 'basic');

    const fieldElements = globalBuilder.fields
        .getElementsByClassName('builder-field-start');
    formData.fields = await Promise.all(
        Array.from(fieldElements, async element => {
            const firstElements = await app.builder.getFormData(element, 'basic');
            const secondElements = await app.builder.getFormData(element.nextSibling, 'basic');
            return {...firstElements, ...secondElements};
        }));

    return formData;
}

document.getElementById('btn-download').addEventListener('click', async () => {
    if (!validateForm()) return;
    const formData = await getFormData();

    // Download json representation of form
    const json = JSON.stringify(formData);
    const dataStr = `data:text/json;charset=utf-8,${encodeURIComponent(json)}`;
    const downloadAnchorNode = document.createElement('a');
    downloadAnchorNode.setAttribute('href', dataStr);
    downloadAnchorNode.setAttribute('download', `${formData.id}.json`);
    document.body.appendChild(downloadAnchorNode);
    downloadAnchorNode.click();
    document.body.removeChild(downloadAnchorNode);
});

document.getElementById('btn-preview').addEventListener('click', async () => {
    if (!validateForm()) return;
    const formData = await getFormData();

    // Save json representation of form and navigate to preview
    sessionStorage.formObject = JSON.stringify(formData);
    window.location = '/form.html?preview=true';
});

document.getElementById('btn-clear').addEventListener('click', () => {
    sessionStorage.formObject = null;
    app.clearForm();
});

document.getElementById('btn-back-from-build').addEventListener('mousedown', () => app.setVisibility('intro'));

try {
    app.editForm(JSON.parse(sessionStorage.formObject));
} catch {}
