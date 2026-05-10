import { formBuilderState as app } from './form_builder_state.js';

app.builder.buildBuilder(app.builderElements);

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

document.getElementById('btn-download').addEventListener('click', async () => {
    if (!validateForm()) return;
    const formData = await app.builder.getFormBuilderData(app.builderElements, 'basic');

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
    const formData = await app.builder.getFormBuilderData(app.builderElements, 'basic');

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
