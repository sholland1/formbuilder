import json
from itertools import product
from datetime import datetime
from pathlib import Path


OUTPUT_PATH = Path("comprehensive-test-form.json")


def state_label(value):
    if value == "missing":
        return "missing"
    return str(value)


def build_text_fields():
    fields = []
    for req_state, placeholder, maxlength, pattern in product(
        ["missing", True, False],
        [False, True],
        [False, True],
        [False, True],
    ):
        labels = [
            f"required-{state_label(req_state)}",
            f"placeholder-{'yes' if placeholder else 'no'}",
            f"maxlength-{'yes' if maxlength else 'no'}",
            f"pattern-{'yes' if pattern else 'no'}",
        ]
        field = {
            "id": "text_" + "_".join(labels),
            "type": "text",
            "question": "Text field: " + ", ".join(labels),
        }
        if req_state != "missing":
            field["required"] = req_state
        if placeholder:
            field["placeholder"] = "(text placeholder)"
        if maxlength:
            field["maxlength"] = 16
        if pattern:
            field["pattern"] = "^[A-Z]{2}[0-9]{2}$"
        fields.append(field)
    return fields


def build_multitext_fields():
    fields = []
    for req_state, placeholder, minp, maxp, maxlength, pattern in product(
        ["missing", True, False],
        [False, True],
        [False, True],
        [False, True],
        [False, True],
        [False, True],
    ):
        labels = [
            f"required-{state_label(req_state)}",
            f"placeholder-{'yes' if placeholder else 'no'}",
            f"min-{'yes' if minp else 'no'}",
            f"max-{'yes' if maxp else 'no'}",
            f"maxlength-{'yes' if maxlength else 'no'}",
            f"pattern-{'yes' if pattern else 'no'}",
        ]
        field = {
            "id": "multitext_" + "_".join(labels),
            "type": "multitext",
            "question": "Multitext field: " + ", ".join(labels),
        }
        if req_state != "missing":
            field["required"] = req_state
        if placeholder:
            field["placeholder"] = "(comma separated values)"
        if minp:
            field["min"] = 1
        if maxp:
            field["max"] = 4
        if maxlength:
            field["maxlength"] = 12
        if pattern:
            field["pattern"] = "^[a-z]+$"
        fields.append(field)
    return fields


def build_number_fields():
    fields = []
    for req_state, minp, maxp, step in product(
        ["missing", True, False],
        [False, True],
        [False, True],
        [False, True],
    ):
        labels = [
            f"required-{state_label(req_state)}",
            f"min-{'yes' if minp else 'no'}",
            f"max-{'yes' if maxp else 'no'}",
            f"step-{'yes' if step else 'no'}",
        ]
        field = {
            "id": "number_" + "_".join(labels),
            "type": "number",
            "question": "Number field: " + ", ".join(labels),
        }
        if req_state != "missing":
            field["required"] = req_state
        if minp:
            field["min"] = 0
        if maxp:
            field["max"] = 100
        if step:
            field["step"] = 0.5
        fields.append(field)
    return fields


def build_select_fields():
    fields = []
    for req_state in ["missing", True, False]:
        label = f"required-{state_label(req_state)}"
        field = {
            "id": "select_" + label,
            "type": "select",
            "question": "Select field: " + label,
            "options": ["alpha", "beta", "gamma"],
        }
        if req_state != "missing":
            field["required"] = req_state
        fields.append(field)
    return fields


def build_multiselect_fields():
    fields = []
    for minp, maxp in product([False, True], [False, True]):
        labels = [
            f"min-{'yes' if minp else 'no'}",
            f"max-{'yes' if maxp else 'no'}",
        ]
        field = {
            "id": "multiselect_" + "_".join(labels),
            "type": "multiselect",
            "question": "Multiselect field: " + ", ".join(labels),
            "options": ["red", "green", "blue", "orange"],
        }
        if minp:
            field["min"] = 1
        if maxp:
            field["max"] = 3
        fields.append(field)
    return fields


def build_timestamp_fields():
    return [{"id": "timestamp_basic", "type": "timestamp"}]


def build_date_fields():
    values = ["missing", True, False]
    dates = ["1900-01-01", "[today]", "2026-03-19", "2099-12-31", "missing"]

    def parse_date(d):
        if d == "missing" or d == "[today]":
            return None
        try:
            return datetime.strptime(d, "%Y-%m-%d").date()
        except ValueError:
            return None

    def end_is_before_start(start_str: str, end_str: str) -> bool:
        start = parse_date(start_str)
        end = parse_date(end_str)

        if start is None or end is None:
            return False                    # Keep combinations with "missing" or invalid dates

        return end < start

    filtered_combinations = [
        (val, start, end)
        for val, start, end in product(values, dates, dates)
        if not end_is_before_start(start, end)
    ]
    fields = []
    for req_state, startp, endp in filtered_combinations:
        labels = [
            f"required-{state_label(req_state)}",
            f"start-date-{startp.removeprefix('[').removesuffix(']')}",
            f"end-date-{endp.removeprefix('[').removesuffix(']')}",
        ]
        field = {
            "id": "date_" + "_".join(labels),
            "type": "date",
            "question": "Date field: " + ", ".join(labels),
        }
        if req_state != "missing":
            field["required"] = req_state
        if startp != "missing":
            field["start_date"] = startp
        if endp != "missing":
            field["end_date"] = endp
        fields.append(field)
    return fields


def build_counter_fields():
    return [{"id": "counter_basic", "type": "counter", "question": "Counter field"}]


def build_color_fields():
    return [{"id": "color_basic", "type": "color", "question": "Color field"}]


def build_bool_fields():
    fields = []
    for req_state in ["missing", True, False]:
        label = f"required-{state_label(req_state)}"
        field = {
            "id": "bool_" + label,
            "type": "bool",
            "question": "Boolean field: " + label,
        }
        if req_state != "missing":
            field["required"] = req_state
        fields.append(field)
    return fields


def build_timer_fields():
    return [{"id": "timer_basic", "type": "timer", "question": "Timer field"}]


def build_fields():
    return (
        build_text_fields()
        + build_multitext_fields()
        + build_number_fields()
        + build_select_fields()
        + build_multiselect_fields()
        + build_timestamp_fields()
        + build_date_fields()
        + build_counter_fields()
        + build_color_fields()
        + build_bool_fields()
        + build_timer_fields()
    )


def main():
    form = {
        "id": "comprehensive-test-form",
        "title": "All Field Permutations",
        "fields": build_fields(),
    }
    bytes = OUTPUT_PATH.write_text(json.dumps(form, indent=4) + "\n")
    print(f"Wrote {bytes} bytes to {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
