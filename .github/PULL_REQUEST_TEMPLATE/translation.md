---
name: Translation
about: Submit a documentation translation for review
title: "i18n(<locale>): Translate <page>.md to <Language>"
labels: ["translation"]
---

## Translation PR Checklist

**Locale (language code):**  <!-- e.g. fr, de, es, pt, ja -->

**Source file(s) translated:**
- [ ] `docs/<page>.md` → `docs/<page>.<locale>.md`

---

### Quality Checklist

- [ ] All prose text is translated (no untranslated paragraphs remaining)
- [ ] Markdown structure is identical to the English source (same headings, tables, code blocks)
- [ ] Technical terms are **not** translated (function names, variable names, API identifiers)
- [ ] Code blocks are **not** translated (inline comments may be translated)
- [ ] Math formulas (`$$...$$`) are unchanged
- [ ] Diagram labels are translated where they contain natural language text
- [ ] No local filesystem links introduced (`file:///...`)
- [ ] All code blocks have a language tag (` ```bash `, ` ```c `, etc.)
- [ ] `mkdocs build` passes locally without errors

### Local Build Verification

```bash
python3 -m mkdocs build
# or
just docs
```

Output:
```
# Paste the last 5 lines of mkdocs build output here
```

---

### Notes for the Maintainer

<!-- Any context, translation decisions, or open questions for review -->
