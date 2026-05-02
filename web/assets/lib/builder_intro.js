import { formBuilderState as app } from './form_builder_state.js';

document.title = app.builderFormObject.title;

document.getElementById('btn-scratch').addEventListener('mousedown', () => app.setVisibility('builder'));
document.getElementById('btn-gen-ai').addEventListener('mousedown', () => app.setVisibility('genai'));
document.getElementById('btn-upload').addEventListener('mousedown', () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.addEventListener('change', () => {
        const file = input.files[0];
        if (!file) return;

        const reader = new FileReader();
        reader.addEventListener('load', () => {
            const currentTime = Temporal.Now.instant();

            const uploadedForm = JSON.parse(reader.result);
            sessionStorage.formObject = JSON.stringify(uploadedForm);

            app.clearForm();
            app.editForm(uploadedForm);

            const diff = Temporal.Now.instant().since(currentTime);
            console.log(diff);
        });
        reader.readAsText(file);
    });
    input.click();
});
