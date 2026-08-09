import { formBuilderState as app } from './form_builder_state.js';

document.title = app.builderFormObject.title;

document.getElementById('btn-scratch').addEventListener('mousedown', () => app.setVisibility('builder'));
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

async function loadPrompt() {
    const response = await fetch('assets/prompt.md');
    return await response.text();
}

async function generateFormJsonWithAI(apiKey, prompt) {
    const url = 'https://api.openai.com/v1/chat/completions';

    app.systemPromptText ??= await loadPrompt();

    const response = await fetch(url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            Authorization: `Bearer ${apiKey}`,
        },
        body: JSON.stringify({
            model: 'gpt-4o-mini',
            messages: [
                { role: 'system', content: app.systemPromptText },
                { role: 'user', content: prompt },
            ],
            temperature: 0.0,
            stream: false,
        }),
    });

    const data = await response.json();
    return JSON.parse(data.choices[0].message.content);
}

if (sessionStorage.apiKey) {
    document.getElementById('api-key').value = sessionStorage.apiKey;
}

document.getElementById('submit-btn').addEventListener('click', async () => {
    const errorMessage = document.getElementById('error-message');
    errorMessage.hidden = true;

    const apiKey = document.getElementById('api-key').value;
    sessionStorage.apiKey = apiKey;

    const form = document.getElementById('prompt-form');
    if (!form.reportValidity()) return;

    document.documentElement.classList.add('wait-cursor');
    form.hidden = true;
    document.getElementById('loading-message').hidden = false;

    const prompt = document.getElementById('prompt').value;
    try {
        const generatedForm = await generateFormJsonWithAI(apiKey, prompt);
        console.log(generatedForm);
        sessionStorage.formObject = JSON.stringify(generatedForm);
        app.clearForm();
        app.editForm(generatedForm);
        form[0].value = '';
    } catch (err) {
        errorMessage.textContent = err.message;
        errorMessage.hidden = false;
        console.log(err);
    } finally {
        document.documentElement.classList.remove('wait-cursor');
        form.hidden = false;
        document.getElementById('loading-message').hidden = true;
    }
});
