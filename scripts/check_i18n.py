#!/usr/bin/env python3
"""Checks the localisation files.
 - every English literal marked with N_("...") or passed to tr("...") in app/*.cpp and the message literals of core/src/{calibration,mixer}.cpp / app/vr_source.cpp
   must have an entry in every lang_XX.inc;
 - every language has exactly the same keys as lang_ru.inc (no stale or missing entries);
 - translations are non-empty, contain no replacement characters / tabs / control characters, keep leading/trailing spaces and punctuation of the key
   (keys like "Could not save the preset: " are concatenated with other text) and keep the same printf placeholders.
Usage: scripts/check_i18n.py [--list-missing LANG]   (exit code 1 on any problem)"""
import re, sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
LANGS = ['ru', 'de', 'fr', 'es']
ent = re.compile(r'^\s*\{"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"\},?\s*$')
def unesc(s): return s.encode('utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8') if '\\' in s else s
def load(l):
    d, bad = {}, []
    for n, line in enumerate((root / f'app/lang_{l}.inc').read_text(encoding='utf-8').splitlines(), 1):
        if not line.strip() or line.lstrip().startswith('//'): continue
        m = ent.match(line)
        if not m: bad.append(f'lang_{l}.inc:{n}: unparsable line'); continue
        k = unesc(m.group(1))
        if k in d: bad.append(f'lang_{l}.inc:{n}: duplicate key {k!r}')
        d[k] = unesc(m.group(2))
    return d, bad
def source_keys():
    keys = set(); lit = r'"((?:[^"\\]|\\.)*)"'
    files = list((root / 'app').glob('*.cpp')) + [root / 'core/src/calibration.cpp', root / 'core/src/mixer.cpp']
    for f in files:
        if f.name in ('selftest.cpp',): continue
        t = f.read_text(encoding='utf-8')
        for m in re.finditer(r'\b(?:N_|tr)\(' + lit + r'\)', t): keys.add(unesc(m.group(1)))
    # messages produced in the core / engine that the UI passes through tr()
    for f in [root / 'core/src/calibration.cpp', root / 'core/src/mixer.cpp', root / 'core/src/ipc_pipe.cpp', root / 'app/vr_source.cpp', root / 'app/app.cpp']:
        t = f.read_text(encoding='utf-8')
        for m in re.finditer(r'\b(?:msg|reason|status|lastError)\s*=\s*' + lit, t): keys.add(unesc(m.group(1)))
        for m in re.finditer(r'\b(?:messages\.push_back|setErr|emplace_back)\(\s*' + lit, t): keys.add(unesc(m.group(1)))
    # role names and scenario names come from arrays in the core
    for f, var in [(root / 'core/src/config.cpp', 'kNames'), (root / 'core/src/sim.cpp', 'kN')]:
        t = f.read_text(encoding='utf-8'); m = re.search(r'\b' + var + r'\[[^\]]*\]\s*=\s*\{(.*?)\};', t, re.S)
        for q in re.finditer(lit, m.group(1)): keys.add(unesc(q.group(1)))
    # ternaries such as tr(cond ? "A" : "B")
    for f in (root / 'app').glob('*.cpp'):
        for m in re.finditer(r'\btr\([^;]*?\?\s*' + lit + r'\s*:\s*' + lit + r'\)', f.read_text(encoding='utf-8')):
            keys.add(unesc(m.group(1))); keys.add(unesc(m.group(2)))
    return keys
def placeholders(s): return sorted(re.findall(r'%[-+ #0]*\d*(?:\.\d+)?[a-zA-Z]', s))
def main():
    problems = []; data = {}
    for l in LANGS:
        d, bad = load(l); data[l] = d; problems += bad
    ref = set(data['ru']) | source_keys()
    if len(sys.argv) > 2 and sys.argv[1] == '--list-missing':
        for k in sorted(ref - set(data[sys.argv[2]])): print(k)
        return 0
    for l in LANGS:
        d = data[l]
        for k in sorted(ref - set(d)): problems.append(f'{l}: missing translation for {k!r}')
        for k in sorted(set(d) - ref): problems.append(f'{l}: stale key (not used by the program) {k!r}')
        for k, v in d.items():
            if not v.strip(): problems.append(f'{l}: empty translation for {k!r}'); continue
            if '�' in v or '\t' in v or any(ord(c) < 32 for c in v): problems.append(f'{l}: bad character in {k!r}')
            if '?' in v and '?' not in k and v.count('?') > 0 and not re.search(r'[¿?]', k): problems.append(f'{l}: unexpected "?" in {v!r}')
            if k.endswith(' ') != v.endswith(' ') or k.startswith(' ') != v.startswith(' '): problems.append(f'{l}: leading/trailing space differs for {k!r}')
            if k.endswith(': ') and not v.endswith(': ') and not v.endswith(' : '): problems.append(f'{l}: {k!r} must end with a colon + space')
            if placeholders(k) != placeholders(v): problems.append(f'{l}: placeholders differ in {k!r}')
            if k == v and len(k) > 24 and l != 'x': pass
    for p in problems: print('I18N:', p)
    print(f'{len(ref)} keys, languages: ' + ', '.join(f'{l}={len(data[l])}' for l in LANGS) + f', problems: {len(problems)}')
    return 1 if problems else 0
sys.exit(main())
