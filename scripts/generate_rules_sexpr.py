#!/usr/bin/env python3
"""Generate RULES_IA.md (S-Expression) from RULES.md.

Usage:
    python3 scripts/generate_rules_sexpr.py RULES.md RULES_IA.md
"""

import re
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sexpr import Node, nil, boolean, integer, string, symbol, keyword, form, list as slist


def clean_md(text: str) -> str:
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
    text = re.sub(r'\*(.+?)\*', r'\1', text)
    text = re.sub(r'\[(.+?)\]\(.+?\)', r'\1', text)
    text = re.sub(r'`(.+?)`', r'\1', text)
    text = re.sub(r'!\[.*?\]\(.*?\)', '', text)
    text = re.sub(r'\s+', ' ', text)
    return text.strip()


def parse_table_row(line: str) -> list[str]:
    parts = line.strip().split('|')
    if parts and parts[0].strip() == '':
        parts = parts[1:]
    if parts and parts[-1].strip() == '':
        parts = parts[:-1]
    return [clean_md(p.strip()) for p in parts]


def is_table_separator(line: str) -> bool:
    s = line.strip()
    if not s.startswith('|'):
        return False
    cells = [c.strip() for c in s.split('|') if c.strip() != '']
    return all(re.match(r'^:?-{3,}:?$', c) for c in cells)


def parse_checklist_line(line: str) -> tuple[bool, str]:
    m = re.match(r'^\s*-\s*\[\s*(x| )?\s*\]\s*(.+)$', line, re.IGNORECASE)
    if m:
        checked = m.group(1).lower() == 'x' if m.group(1) else False
        text = clean_md(m.group(2).strip())
        return (checked, text)
    return (False, clean_md(line.strip()))


def read_file(path: str) -> str:
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()


def parse_sections(lines: list[str]) -> list[dict]:
    sections = []
    current_section: Optional[dict] = None
    current_sub: Optional[dict] = None

    for line in lines:
        m2 = re.match(r'^##\s+(.+)$', line)
        m3 = re.match(r'^###\s+(.+)$', line)

        if m2:
            current_sub = None
            title = clean_md(m2.group(1))
            id_match = re.match(r'(\d+|[A-Z])[.\s]', title)
            sid = id_match.group(1) if id_match else title.lower().replace(' ', '-')
            current_section = {
                'id': sid, 'title': title, 'content': [],
                'subsections': [], 'type': None
            }
            if sid.upper() in ('A',):
                current_section['type'] = 'CHECKLIST'
            sections.append(current_section)
        elif m3:
            title = clean_md(m3.group(1))
            id_match = re.match(r'(\d+\.\d+)[.\s]', title)
            sid = id_match.group(1) if id_match else title.lower().replace(' ', '-')
            current_sub = {
                'id': sid, 'title': title,
                'content': [], 'type': None
            }
            if current_section is not None:
                current_section['subsections'].append(current_sub)
        else:
            target = current_sub if current_sub is not None else current_section
            if target is not None:
                target['content'].append(line)

    return sections


def classify_and_split_content(content_lines: list[str]) -> list[dict]:
    blocks = []
    i = 0
    n = len(content_lines)

    while i < n:
        line = content_lines[i].rstrip('\n')

        if line.strip() == '' or line.strip() == '---':
            i += 1
            continue

        if line.startswith('```'):
            lang = line[3:].strip()
            code: list[str] = []
            i += 1
            while i < n and not content_lines[i].startswith('```'):
                code.append(content_lines[i].rstrip('\n'))
                i += 1
            i += 1
            blocks.append({'type': 'code', 'lang': lang, 'code': '\n'.join(code)})
            continue

        if line.strip().startswith('|') and i + 1 < n and is_table_separator(content_lines[i + 1]):
            header = parse_table_row(line)
            i += 2
            rows: list[list[str]] = []
            while i < n:
                cl = content_lines[i].strip()
                if cl.startswith('|'):
                    rows.append(parse_table_row(cl))
                    i += 1
                else:
                    break
            blocks.append({'type': 'table', 'header': header, 'rows': rows})
            continue

        checklist_lines: list[tuple[bool, str]] = []
        while i < n and re.match(r'^\s*-\s*\[\s*[ x]?\s*\]', content_lines[i], re.IGNORECASE):
            checked, text = parse_checklist_line(content_lines[i])
            checklist_lines.append((checked, text))
            i += 1
        if checklist_lines:
            blocks.append({'type': 'checklist', 'items': checklist_lines})
            continue

        if line.strip().startswith('- ') or line.strip().startswith('* '):
            items: list[str] = []
            while i < n:
                cl = content_lines[i].strip()
                if cl.startswith('- ') or cl.startswith('* '):
                    items.append(clean_md(cl[2:]))
                    i += 1
                else:
                    break
            blocks.append({'type': 'ulist', 'items': items})
            continue

        om = re.match(r'^\s*(\d+)[.)]\s+(.*)', line)
        if om:
            oitems: list[str] = []
            while i < n:
                cl = content_lines[i].strip()
                mo = re.match(r'^\s*(\d+)[.)]\s+(.*)', cl)
                if mo:
                    oitems.append(clean_md(mo.group(2)))
                    i += 1
                else:
                    break
            blocks.append({'type': 'olist', 'items': oitems})
            continue

        if line.strip().startswith('> '):
            quotes: list[str] = []
            while i < n and content_lines[i].strip().startswith('> '):
                quotes.append(content_lines[i].strip()[2:])
                i += 1
            blocks.append({'type': 'quote', 'text': clean_md(' '.join(quotes))})
            continue

        para: list[str] = []
        while i < n:
            cl = content_lines[i].rstrip('\n')
            if cl.strip() == '' or cl.strip() == '---':
                break
            if any(cl.strip().startswith(p) for p in ('```', '|', '- ', '* ', '> ')):
                break
            if re.match(r'^\s*\d+[.)]\s', cl):
                break
            if re.match(r'^\s*-\s*\[\s*[ x]?\s*\]', cl):
                break
            if cl.strip().startswith('##'):
                break
            para.append(cl)
            i += 1

        if para:
            text = clean_md(' '.join(para))
            if text:
                blocks.append({'type': 'body', 'text': text})
            continue

        i += 1

    return blocks


def build_section_sexpr(section: dict) -> Node:
    n = form('section')
    n.kv('id', string(section['id']))
    n.kv('title', string(section['title']))

    has_content = bool(section.get('content'))
    has_subs = bool(section.get('subsections'))

    if not has_subs and has_content:
        blocks = classify_and_split_content(section['content'])
        _add_blocks(n, blocks)
    elif has_content:
        blocks = classify_and_split_content(section['content'])
        _add_blocks(n, blocks)

    if section.get('type'):
        n.kv('type', keyword(section['type']))

    if has_subs:
        subs = slist([])
        for sub in section['subsections']:
            subs.push(build_subsection_sexpr(sub))
        n.kv('subsections', subs)

    return n


def build_subsection_sexpr(sub: dict) -> Node:
    n = form('subsection')
    n.kv('id', string(sub['id']))
    n.kv('title', string(sub['title']))

    blocks = classify_and_split_content(sub['content'])
    _add_blocks(n, blocks)

    if sub.get('type'):
        n.kv('type', keyword(sub['type']))

    return n


def _add_blocks(n: Node, blocks: list[dict]):
    if not blocks:
        return

    body_parts: list[str] = []
    has_structured = False

    for block in blocks:
        if block['type'] == 'body':
            if block['text']:
                body_parts.append(block['text'])
        else:
            has_structured = True

    if body_parts:
        n.kv('body', string('\n\n'.join(body_parts)))

    if has_structured:
        items = slist([])
        for block in blocks:
            if block['type'] == 'body':
                continue
            items.push(_emit_block(block))
        n.kv('content', items)


def _emit_block(block: dict) -> Node:
    t = block['type']
    if t == 'code':
        bn = form('code-example')
        bn.kv('lang', string(block.get('lang', '')))
        bn.kv('code', string(block['code']))
        return bn
    elif t == 'table':
        bn = form('table')
        bn.kv('schema', slist([string(h) for h in block['header']]))
        rows = slist([])
        for row in block['rows']:
            rn = form('row')
            for h, v in zip(block['header'], row):
                rn.kv(h, string(v))
            rows.push(rn)
        bn.kv('rows', rows)
        return bn
    elif t == 'checklist':
        bn = form('checklist')
        items = slist([])
        for idx, (checked, text) in enumerate(block['items'], 1):
            ci = form('item')
            ci.kv('id', integer(idx))
            ci.kv('text', string(text))
            ci.kv('done', boolean(checked))
            items.push(ci)
        bn.kv('items', items)
        return bn
    elif t == 'ulist':
        bn = form('unordered-list')
        items = slist([])
        for it in block['items']:
            items.push(string(it))
        bn.kv('items', items)
        return bn
    elif t == 'olist':
        bn = form('ordered-list')
        items = slist([])
        for it in block['items']:
            items.push(string(it))
        bn.kv('items', items)
        return bn
    elif t == 'quote':
        bn = form('quote')
        bn.kv('text', string(block['text']))
        return bn
    return form('unknown')


def extract_foreword(lines: list[str]) -> dict:
    foreword_lines: list[str] = []
    for line in lines:
        if line.startswith('## '):
            break
        stripped = line.strip()
        if stripped.startswith('> '):
            stripped = stripped[2:]
        if stripped and stripped != '---':
            foreword_lines.append(stripped)
    text = clean_md(' '.join(foreword_lines))
    return {'text': text}


def extract_principles(sections: list[dict]) -> list[dict]:
    for sec in sections:
        if sec['id'] == '1':
            for sub in sec.get('subsections', []):
                if '1.2' in sub['id']:
                    blocks = classify_and_split_content(sub['content'])
                    principles = []
                    for block in blocks:
                        if block['type'] == 'olist':
                            for item in block['items']:
                                ptype = 'MUST' if any(w in item.upper() for w in ['UNICO', 'PROIBIDO', 'EXIGIDO', 'OBRIGATORIO', 'NUNCA', 'APENAS']) else 'SHOULD'
                                principles.append({'id': len(principles) + 1, 'text': item, 'type': ptype})
                    return principles
    return []


def generate(input_path: str, output_path: str):
    text = read_file(input_path)
    lines = text.split('\n')

    foreword = extract_foreword(lines)
    sec_lines = [l for l in lines if not l.startswith('> ') or l.startswith('##') or l.startswith('###')]
    sections = parse_sections(lines)

    title_line = lines[0] if lines else ''
    doc_title = clean_md(title_line.lstrip('# ')) if title_line.startswith('# ') else 'RULES.md'

    root = form('scout:rules')
    root.kv('version', string('1.0'))
    root.kv('generated', string(__import__('datetime').datetime.now(__import__('datetime').timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')))
    root.kv('source', string(os.path.basename(input_path)))

    proj = form('project')
    proj.kv('name', string('Scout++'))
    proj.kv('tagline', string('Agent-First Forensic Framework'))
    proj.kv('language', string('C++26'))
    proj.kv('output-format', string('S-Expression'))

    purpose = ''
    for sec in sections:
        if sec['id'] == '1':
            for sub in sec.get('subsections', []):
                if '1.1' in sub['id']:
                    blocks = classify_and_split_content(sub['content'])
                    for b in blocks:
                        if b['type'] == 'body':
                            purpose = b['text']
                            break
    if purpose:
        proj.kv('purpose', string(purpose))

    principles = extract_principles(sections)
    if principles:
        plist = slist([])
        for p in principles:
            pn = form('principle')
            pn.kv('id', integer(p['id']))
            pn.kv('text', string(p['text']))
            pn.kv('type', keyword(p['type']))
            plist.push(pn)
        proj.kv('principles', plist)

    root.kv('project', proj)

    if foreword.get('text'):
        root.kv('foreword', string(foreword['text']))

    root.kv('sections', slist([build_section_sexpr(s) for s in sections]))

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(root.to_string(pretty=True))
        f.write('\n')

    print(f'Gerado: {output_path}')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'Uso: {sys.argv[0]} <RULES.md> <RULES_IA.md>', file=sys.stderr)
        sys.exit(1)
    generate(sys.argv[1], sys.argv[2])
