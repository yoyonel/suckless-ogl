#!/usr/bin/env python3
import os
import re
import sys
from pathlib import Path

def convert_doxygen_to_mkdocs(content: str, filename: str) -> str:
    """
    Converts Doxygen Markdown syntax to MkDocs/CommonMark syntax.
    """

    # 1. Convert \code{.ext} to ```ext
    # Pattern: \code{.c} ... \endcode
    def replace_code_block(match):
        lang = match.group(1) if match.group(1) else ""
        code = match.group(2)
        # Remove leading dot if present in language extension (e.g. .c -> c)
        if lang.startswith('.'):
            lang = lang[1:]
        return f"```{lang}\n{code}```"

    # Regex for \code{.ext} or \code
    # syntax: \code{.c} OR \code
    # We match \code(?:\{(\.\w+)\})?(.+?)\\endcode
    # FLAGS: DOTALL to match newlines
    content = re.sub(r'\\code(?:\{\s*(\.\w+)\s*\})?\n?(.+?)\\endcode', replace_code_block, content, flags=re.DOTALL)

    # Handle simple \code without braces if the above didn't catch it all (or mixed usage)
    # The previous regex handles \code{.c} and also handles \code (group 1 is None) if we write it effectively.
    # But often Doxygen uses \code ... \endcode without extension.
    # Let's verify the first regex covers it.
    # The regex `\\code(?:\{\s*(\.\w+)\s*\})?` expects `{ .ext }` optionally.
    # If standard doxygen uses `\code` plain, it matches `(?:...)?` as empty.

    # 2. Convert \dot ... \enddot to ```graphviz (or whatever the plugin expects)
    # Using 'graphviz' as language identifier for kroki or mkdocs-graphviz
    def replace_dot_block(match):
        code = match.group(1)
        return f"```graphviz\n{code}```"

    content = re.sub(r'\\dot\n?(.+?)\\enddot', replace_dot_block, content, flags=re.DOTALL)

    # 3. Convert \ref
    # Pattern: \ref class_name "Link Text" or just \ref class_name
    # Getting accurate links is hard without a full symbol table.
    # We will try to convert them to [Link Text](class_name) and hope relative linking works
    # or just keep them as text if elusive.
    # Best effort: \ref <ID> "<Text>" -> [<Text>](<ID>.md)
    # Note: Doxygen generates HTML, so file extensions are hidden. We assume we might map to .md files if they exist.

    # Case 1: \ref ID "Text"
    content = re.sub(r'\\ref\s+(\w+)\s+"([^"]+)"', r'[\2](\1.md)', content)

    # Case 2: \ref ID
    content = re.sub(r'\\ref\s+(\w+)', r'[\1](\1.md)', content)

    # 4. Convert \note, \warning, etc. into Admonitions
    # Doxygen: \note text...
    # MkDocs: !!! note
    #             text...
    # This is tricky because Doxygen commands might not end clearly. Usually they end at the paragraph end.
    # Simple strategy: replace `\note` with `!!! note\n    ` and assume the user formats it manually or usage is simple.
    # However, Doxygen might use `\note` inline.
    # Let's just fix the block level ones which are common at start of lines.

    def replace_admonition(match):
        type_ = match.group(1).lower()
        text = match.group(2)
        return f"!!! {type_}\n    {text}"

    # Regex: ^\s*\\(note|warning|attention)\s+(.*)
    # We apply this line by line? No, multi-line notes are harder.
    # Let's stick to simple replacements for now or manual fix.
    # Actually, Doxygen often uses \note followed by text using indentation.

    # Let's leave \note as is? Or try a simple replace.
    # content = re.sub(r'\\(note|warning|attention|bug)', r'!!! \1', content)
    # Be careful with indentation.

    # 5. Fix \page and \mainpage
    # \mainpage usually in README or distinct file. We might want to remove it.
    content = re.sub(r'\\mainpage.*', '', content)
    content = re.sub(r'\\page\s+\w+\s+.*', '', content)

    # 6. Basic formatting
    # \b word -> **word**
    content = re.sub(r'\\b\s+(\w+)', r'**\1**', content)

    # \c word -> `word`
    content = re.sub(r'\\c\s+(\w+)', r'`\1`', content)

    return content

def main():
    root_dir = Path("docs")
    if not root_dir.exists():
        print(f"Directory {root_dir} not found.")
        sys.exit(1)

    files = list(root_dir.glob("**/*.md"))
    print(f"Found {len(files)} markdown files to process.")

    for file_path in files:
        # separate exclude check
        if "doxygen-awesome-css" in str(file_path):
            continue

        print(f"Processing {file_path}...")
        try:
            with open(file_path, "r", encoding="utf-8") as f:
                content = f.read()

            new_content = convert_doxygen_to_mkdocs(content, file_path.name)

            if new_content != content:
                with open(file_path, "w", encoding="utf-8") as f:
                    f.write(new_content)
                print(f"  Modified.")
            else:
                print(f"  No changes needed.")

        except Exception as e:
            print(f"Error processing {file_path}: {e}")

if __name__ == "__main__":
    main()
