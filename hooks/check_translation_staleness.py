"""
MkDocs hook: Translation Staleness & Missing-Translation Checker
=================================================================
Two features:

1. **Stale translation warning** — injected into translated pages (``page.fr.md``)
   when the English source has been modified more recently than the translation.

2. **Missing translation notice** — injected into pages served under a locale URL
   (e.g. ``/fr/``) that have *no* corresponding ``.fr.md`` file and therefore
   fall back silently to showing English content.

Detection logic for staleness (in order of preference):
1. Git commit timestamp of each file (most accurate, requires git history)
2. Filesystem mtime as fallback (e.g., for newly created files not yet committed)

The hook runs on the on_page_markdown event and prepends admonitions to the
page content when either condition is detected.
"""

import os
import subprocess
import logging
from pathlib import Path

log = logging.getLogger("mkdocs.hooks.translation_staleness")

STALE_WARNING = """\
> [!WARNING]
> **This translation may be outdated.** The English source has been updated more recently than this page.
> If you notice inaccuracies, please consider [contributing an update](../contributing_translations/).

---

"""

MISSING_TRANSLATION = """\
> [!NOTE]
> **This page has not been translated yet.** You are reading the original English version.
> Want to help? See the [translation contribution guide](../contributing_translations/).

---

"""

# Locale suffixes that identify translated files (e.g. ".fr", ".de")
TRANSLATION_SUFFIXES = {".fr", ".de", ".es", ".pt", ".ja", ".zh"}


def _git_mtime(filepath: str) -> float | None:
    """Return the Unix timestamp of the last git commit touching this file, or None."""
    try:
        result = subprocess.run(
            ["git", "log", "-1", "--format=%ct", "--", filepath],
            capture_output=True,
            text=True,
            cwd=os.path.dirname(filepath) or ".",
        )
        raw = result.stdout.strip()
        return float(raw) if raw else None
    except (FileNotFoundError, ValueError):
        return None


def _effective_mtime(filepath: str) -> float:
    """Return the most reliable mtime: git commit time if available, else fs mtime."""
    git_ts = _git_mtime(filepath)
    if git_ts is not None:
        return git_ts
    return os.path.getmtime(filepath)


def _is_stale(translated_path: str, source_path: str) -> bool:
    """Return True if the English source is newer than the translation."""
    if not os.path.exists(source_path):
        return False
    if not os.path.exists(translated_path):
        return False
    source_mtime = _effective_mtime(source_path)
    translated_mtime = _effective_mtime(translated_path)
    # A grace period of 60 seconds to avoid false positives on simultaneous edits
    return source_mtime > translated_mtime + 60


def on_page_markdown(markdown, page, config, files):
    """Inject staleness or missing-translation notices into locale pages."""
    src_path = page.file.src_path       # e.g. "banding.fr.md" or "auto_exposure.md"
    dest_uri = page.file.dest_uri       # e.g. "fr/banding/index.html"
    docs_dir = config["docs_dir"]

    # ------------------------------------------------------------------ #
    # Case 1: This is a translated page — check for staleness             #
    # ------------------------------------------------------------------ #
    file_stem = Path(src_path).stem     # "banding.fr"
    parts = file_stem.rsplit(".", 1)    # ["banding", "fr"]

    if len(parts) == 2:
        base_name, locale = parts
        if f".{locale}" in TRANSLATION_SUFFIXES:
            translated_file = os.path.join(docs_dir, src_path)
            source_file = os.path.join(docs_dir, f"{base_name}.md")

            if os.path.exists(source_file) and _is_stale(translated_file, source_file):
                log.info(
                    "Translation stale: %s is older than %s",
                    os.path.basename(translated_file),
                    os.path.basename(source_file),
                )
                return STALE_WARNING + markdown
            return markdown

    # ------------------------------------------------------------------ #
    # Case 2: Fallback English page served under a locale URL             #
    # e.g. dest_uri = "fr/auto_exposure/index.html",                      #
    #      src_path = "auto_exposure.md"  (no locale suffix)              #
    # ------------------------------------------------------------------ #
    locale_prefixes = {suffix.lstrip(".") + "/" for suffix in TRANSLATION_SUFFIXES}
    for prefix in locale_prefixes:
        if dest_uri.startswith(prefix):
            # Page is served under a locale URL but src is the English source
            log.info(
                "No translation found for %s (locale: %s) — injecting notice.",
                src_path,
                prefix.rstrip("/"),
            )
            return MISSING_TRANSLATION + markdown

    return markdown
