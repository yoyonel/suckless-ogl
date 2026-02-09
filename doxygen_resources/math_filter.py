import sys
import re

def filter_math(content):
    # Convert block math: $$ ... $$ to \f[ ... \f]
    content = re.sub(r'\$\$(.*?)\$\$', r'\\f[\1\\f]', content, flags=re.DOTALL)

    # Convert inline math: $ ... $ to \f$ ... \f$
    # Matches $...$ but avoids $$...$$, escaped \$, and ensure something is inside
    content = re.sub(r'(?<!\$)(?<!\\)\$(?!\$)([^\$]+?)(?<!\\)\$(?!\$)', r'\\f$\1\\f$', content)

    return content

if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(0)

    filename = sys.argv[1]
    try:
        with open(filename, 'r', encoding='utf-8') as f:
            content = f.read()
        sys.stdout.write(filter_math(content))
    except Exception:
        # Fallback to outputting original if error
        with open(filename, 'r', encoding='utf-8') as f:
            sys.stdout.write(f.read())
