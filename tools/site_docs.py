"""Publish the maintained guides and public source alongside the static installer.

Requires markdown-it-py==4.2.0. Never walk the working tree for source archives:
git archive includes tracked source only, excluding credentials, logs and backups.
"""
from pathlib import Path
from urllib.parse import urlsplit, unquote
import argparse
import html
import os
import re
import shutil
import subprocess

from markdown_it import MarkdownIt

STYLE = '''body{max-width:960px;margin:0 auto;padding:28px;font:17px/1.65 system-ui,sans-serif;background:#101622;color:#eef2ff}a{color:#72e9d5}img{max-width:100%;height:auto}pre{overflow:auto;padding:18px;background:#070d17;border-radius:12px}code{font-size:.9em}table{display:block;overflow:auto;border-collapse:collapse}td,th{padding:10px;border:1px solid #40506a}h1,h2,h3{line-height:1.2;margin-top:1.6em}nav{display:flex;gap:24px;flex-wrap:wrap;border-bottom:1px solid #40506a;padding-bottom:18px}blockquote{border-left:3px solid #72e9d5;margin-left:0;padding-left:18px}'''


def build_docs(root, output, repo='Dash-Chan', home='https://valleytech-dash-chan.pages.dev/', download='firmware/Dash-Chan.zip'):
    root, output = Path(root).resolve(), Path(output).resolve()
    guide = output / 'guide'
    guide.mkdir(parents=True, exist_ok=True)
    documents = sorted(root.glob('*.md')) + sorted((root / 'docs').rglob('*.md'))
    if (root / 'LICENSE').is_file():
        documents.append(root / 'LICENSE')
    generated = {p.relative_to(root).as_posix(): p.relative_to(root).with_suffix('.html').as_posix() for p in documents}
    (guide / 'guide.css').write_text(STYLE, encoding='utf-8')

    def local_url(target, current):
        return os.path.relpath(output / target, current.parent).replace('\\', '/')

    for source in documents:
        relative = source.relative_to(root).as_posix()
        destination = guide / generated[relative]
        destination.parent.mkdir(parents=True, exist_ok=True)

        def rewrite(url):
            parsed = urlsplit(html.unescape(url))
            if url.startswith('#') or parsed.scheme in ('mailto', 'tel', 'data'):
                return url
            path = unquote(parsed.path)
            suffix = ('?' + parsed.query if parsed.query else '') + ('#' + parsed.fragment if parsed.fragment else '')
            if parsed.netloc == 'valleytechsolutions.github.io' and path.startswith('/' + repo + '/'):
                return home + path[len(repo) + 2:] + suffix
            if parsed.netloc == 'github.com' and path.startswith('/valleytechsolutions/' + repo):
                tail = path[len('/valleytechsolutions/' + repo):]
                if tail.startswith('/releases'):
                    return local_url(download, destination) + suffix
                match = re.match(r'/(?:blob|tree)/[^/]+/(.+)', tail)
                if match:
                    path = match[1]
                elif not tail or tail == '/':
                    return local_url('source.zip', destination)
                else:
                    return url
            elif parsed.netloc or parsed.scheme:
                return url
            else:
                resolved = (source.parent / path).resolve()
                if not resolved.is_relative_to(root):
                    return url
                path = resolved.relative_to(root).as_posix()
            if path in generated:
                return local_url('guide/' + generated[path], destination) + suffix
            candidate = (root / path).resolve()
            if candidate.is_relative_to(root) and candidate.is_file():
                target = guide / path
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(candidate, target)
                return local_url('guide/' + path, destination) + suffix
            return url

        renderer = MarkdownIt('commonmark', {'html': True}).enable('table')
        text = source.read_text(encoding='utf-8')
        if source.name == 'LICENSE':
            body = '<h1>License</h1><pre>' + html.escape(text) + '</pre>'
        else:
            tokens = renderer.parse(text)
            used = set()
            for i, token in enumerate(tokens):
                if token.type == 'heading_open':
                    slug = re.sub(r'[^\w\- ]', '', tokens[i + 1].content.lower()).replace(' ', '-')
                    original, n = slug, 1
                    while slug in used:
                        slug, n = original + '-' + str(n), n + 1
                    used.add(slug)
                    token.attrSet('id', slug)
            body = renderer.renderer.render(tokens, renderer.options, {})
            body = re.sub(r'\b(href|src)="([^"]+)"', lambda m: m[1] + '="' + html.escape(rewrite(m[2]), quote=True) + '"', body)
        title = html.escape(source.stem.replace('-', ' '))
        css = local_url('guide/guide.css', destination)
        nav = '<nav><a href="' + local_url('index.html', destination) + '">Installer & companion tools</a><a href="' + local_url('guide/index.html', destination) + '">All guides</a><a href="' + local_url('source.zip', destination) + '">Download source</a></nav>'
        destination.write_text('<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>' + title + ' · ' + html.escape(repo) + '</title><link rel="stylesheet" href="' + css + '"></head><body>' + nav + '<main>' + body + '</main></body></html>', encoding='utf-8')
    links = ''.join('<li><a href="' + html.escape(target) + '">' + html.escape(name) + '</a></li>' for name, target in generated.items())
    (guide / 'index.html').write_text('<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Guides · ' + html.escape(repo) + '</title><link rel="stylesheet" href="guide.css"></head><body><nav><a href="../">Back to installer</a><a href="../source.zip">Download source</a></nav><h1>' + html.escape(repo) + ' guides</h1><p>Guides and attributions shipped with this deployment.</p><ul>' + links + '</ul></body></html>', encoding='utf-8')
    print('Published', len(documents), 'local guides for', repo)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', default='site')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = (root / args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', 'archive', '--format=zip', '--output=' + str(output / 'source.zip'), 'HEAD'], cwd=root, check=True)
    build_docs(root, output)
