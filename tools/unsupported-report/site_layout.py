"""Shared header and document layout for the integrated Pages reports."""

from html import escape


def document(title: str, inner: str, *, detail: bool = False) -> str:
    root = "../../" if detail else "../"
    projects = "../index.html" if detail else "index.html"
    current = "location" if detail else "page"
    heading = escape(title)
    return (
        '<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
        f"<title>{heading} | VerihoggFormat reports</title>\n"
        f'<link rel="stylesheet" href="{root}sv-tests-reports.css">\n'
        '</head>\n<body>\n<header class="report-header">\n'
        f'<p class="report-brand"><a href="{root}index.html">verihogg <span>/ reports</span></a></p>\n'
        f"<h2>{heading}</h2>\n"
        '<nav class="report-nav" aria-label="Construct reports">'
        f'<a href="{root}unsupported.html">Full report</a>'
        f'<a href="{root}constructs.html">Warning constructs</a>'
        f'<a href="{projects}" aria-current="{current}">In projects</a></nav>\n'
        '</header>\n<main class="report-content">\n'
        f"{inner}\n</main>\n</body>\n</html>\n"
    )
