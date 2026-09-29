"""Build-only HTML/CSS compaction. Templates keep descriptive private class names.

Only static classes with no occurrence in JavaScript may be shortened. Public
DOM IDs, JS selectors, all content and all executable code stay unchanged.
"""
from collections import Counter
from html.parser import HTMLParser
import re

class _Classes(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=False)
        self.names = Counter()
    def handle_starttag(self, tag, attrs):
        for name, value in attrs:
            if name == 'class' and value:
                self.names.update(value.split())

def compact_html(html):
    parser = _Classes()
    parser.feed(html)
    scripts = '\n'.join(re.findall(r'<script\b[^>]*>(.*?)</script\s*>', html, re.S | re.I))
    names = parser.names.keys()
    eligible = sorted((name for name in names if name not in scripts and not name.startswith('ym-')), key=lambda name: (-html.count(name), name))
    mapping = {}
    index = 0
    for name in eligible:
        while True:
            short = ''
            n = index
            while True:
                short = chr(97 + n % 26) + short
                n = n // 26 - 1
                if n < 0:
                    break
            index += 1
            if short not in names:
                break
        mapping[name] = short
    parts = re.split(r'(<script\b[^>]*>.*?</script\s*>|<style\b[^>]*>.*?</style\s*>)', html, flags=re.S | re.I)
    for i, part in enumerate(parts):
        if part.lower().startswith('<script'):
            continue
        if part.lower().startswith('<style'):
            # This project's CSS uses simple, unescaped ASCII class selectors.
            # One substitution pass prevents generated names being remapped.
            parts[i] = re.sub(r'\.([a-zA-Z_][\w-]*)', lambda m: '.' + mapping.get(m[1], m[1]), part)
        else:
            parts[i] = re.sub(r'class="([^"]*)"', lambda m: 'class="' + ' '.join(mapping.get(name, name) for name in m[1].split()) + '"', part)
    return ''.join(parts)
