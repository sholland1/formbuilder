import json
from pathlib import Path


INPUT_PATH = Path("web/assets/builder.json")
OUTPUT_PATH = Path("web/assets/prompt.md")


# Properties that every field shares — excluded from per-type documentation
# because they're covered in the "Every field has" section.
UNIVERSAL_PROPS = {"id", "type"}

# Human-readable descriptions for well-known property ids.
# Extend this dict if builder.json grows new property ids.
PROP_DESCRIPTIONS: dict[str, str] = {
    "label":    "the label/prompt shown to the user",
    "required":    "whether the field is required (default true)",
    "placeholder": "placeholder hint text",
    "maxlength":   "maximum character length",
    "pattern":     "regex the value must match",
    "min":         "minimum value / minimum number of entries / minimum selections",
    "max":         "maximum value / maximum number of entries / maximum selections",
    "step":        "numeric step size",
    "options":     "list of available choices (json array)",
    "start_date":  "earliest allowed date 'YYYY-MM-DD' or '[today]' to indicate the current date",
    "end_date":    "latest allowed date 'YYYY-MM-DD' or '[today]' to indicate the current date",
    "maxsize":     "maximum file size in KB",
    "fileexts":    "list of allowed file extensions, each starts with a period (json array)",
}

# Human-readable descriptions for each field type.
TYPE_DESCRIPTIONS: dict[str, str] = {
    "text":        "Single-line text input.",
    "multitext":   "Collects a comma-separated list of text entries.",
    "number":      "Numeric input.",
    "select":      "Single-choice dropdown.",
    "multiselect": "Multi-choice selector.",
    "bool":        "Yes/No toggle.",
    "date":        "Date picker.",
    "timestamp":   "Automatically captures the current date and time when the form is submitted. No additional properties needed — do not add any.",
    "counter":     "Integer increment/decrement counter.",
    "color":       "Color picker.",
    "timer":       "Measures elapsed time (e.g. for timed tasks).",
    "guid":        "Automatically generates a GUID when the form is submitted. No additional properties needed — do not add any.",
    "file":        "File picker.",
    "signature":   "Allows the user to draw their name with the mouse or touchscreen.",
}


def field_meta(field: dict) -> str:
    """Return a one-line markdown description of a single field definition."""
    fid = field["id"]
    ftype = field["type"]
    is_required = field.get("required", True)
    required_tag = "" if is_required else " *(optional)*"

    description = PROP_DESCRIPTIONS.get(fid, "")

    # For select/multiselect fields whose options are the legal values, list them.
    if ftype == "select" and "options" in field:
        opts = ", ".join(f'"{o}"' for o in field["options"])
        description = f"one of {opts}"
    elif ftype == "number":
        constraints = []
        if "max" in field:
            constraints.append(f"max {field['max']}")
        if constraints:
            description = (description + "; " if description else "") + ", ".join(constraints)

    desc_part = f": {description}" if description else ""
    return f"- **`{fid}`** ({ftype}){required_tag}{desc_part}"


def build_type_section(type_key: str, form_def: dict) -> str:
    """
    Return the markdown documentation block for one field type,
    derived entirely from the builder.json sub-form definition.
    """
    type_name = type_key.removeprefix("field_type_")
    fields = form_def.get("fields", [])

    header = f"### type: `\"{type_name}\"`"
    blurb = TYPE_DESCRIPTIONS.get(type_name, "")
    if blurb:
        header = f"{header}\n{blurb}"

    # Filter out universal props — they're documented globally
    doc_fields = [f for f in fields if f["id"] not in UNIVERSAL_PROPS]

    if not doc_fields:
        return header  # e.g. timestamp — no extra properties

    lines = [header, ""]
    for field in doc_fields:
        lines.append(field_meta(field))

    return "\n".join(lines)


def generate_prompt(builder: dict) -> str:
    """Derive and return the full system prompt from a builder.json structure."""

    # Collect all type names from field_start's type selector
    field_start = builder["forms"].get("field_start", {})
    type_field = next((f for f in field_start.get("fields", []) if f["id"] == "type"), None)
    all_types: list[str] = type_field["options"] if type_field else []

    # Build per-type sections (only for keys present in builder.json)
    type_sections: list[str] = []
    for t in all_types:
        key = f"field_type_{t}"
        if key in builder["forms"]:
            type_sections.append(build_type_section(key, builder["forms"][key]))

    type_block = "\n\n---\n\n".join(type_sections)

    # Build the legal `step` option list from field_type_number, if present
    step_opts = ""
    number_form = builder["forms"].get("field_type_number", {})
    step_field = next((f for f in number_form.get("fields", []) if f["id"] == "step"), None)
    if step_field and "options" in step_field:
        step_opts = ", ".join(f'"{o}"' for o in step_field["options"])

    prompt = f"""\
You are a form schema generator. Your job is to output valid JSON representing \
a form, based on the user's description.

## Output Format

Return ONLY a raw JSON object — no markdown, no backticks, no explanation. \
The JSON must match this schema exactly:

{{
  "id": string,       // kebab-case identifier, e.g. "contact-form"
  "title": string,    // human-readable form title
  "fields": [ ...FieldObject ]
}}

## Field Types and Their Properties

Every field object has two universal properties:
- **`id`** (text): a unique camelCase or kebab-case identifier for the field, \
starting with a letter and containing only letters, numbers, underscores, or hyphens.
- **`type`** (select): one of: {", ".join(f'`"{t}"`' for t in all_types)}

Additional properties depend on the type:

---

{type_block}

## Rules

1. Always include exactly one `"timestamp"` field to record submission time. \
It must have no properties other than `"id"` and `"type"`. \
It should be the last field.
2. Use clear, natural language for `"label"` values.
3. Choose field types that best match the data being collected.
4. Only include properties that are listed for the chosen field type — \
do not invent or add extra keys.
5. All field `"id"` values must be unique within the form.
6. Output raw JSON only. No commentary, no markdown fences.
7. Make sure ranges are coherent: \
min is lower than max \
start_date is earlier than end_date
"""

    if step_opts:
        # Inject the legal step values into the prompt near the number section
        prompt = prompt.replace(
            "- **`step`** (select)*(optional)*",
            f"- **`step`** (select)*(optional)*: one of {step_opts}",
        )

    return prompt.strip()


def main() -> None:
    if not INPUT_PATH.exists():
        raise FileNotFoundError(f"builder.json not found at: {INPUT_PATH}")

    with INPUT_PATH.open() as f:
        builder = json.load(f)

    prompt = generate_prompt(builder)

    bytes = OUTPUT_PATH.write_text(prompt)
    print(f"Wrote {bytes} bytes to {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
