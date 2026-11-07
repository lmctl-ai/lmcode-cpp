#!/usr/bin/env python3
"""Copy the shared REST conformance suite (the opt-in `rest` std module) from the spec repository and pin it.

  python3 scripts/sync-rest.py            copy ../lmcode/tests/rest-suite (or $LMCODE_SPEC_DIR) to conformance/rest-suite, write rest-suite.lock
  python3 scripts/sync-rest.py --verify   check the copy (and stdlib/rest.schema-fragment.json) against the lock

Digest: sha256 over rows "<sha256(file)>  <relative path>\\n", paths sorted bytewise (identical to
(cd DIR && find . -type f -printf '%P\\n' | LC_ALL=C sort | xargs sha256sum) | sha256sum).
"""
import hashlib, os, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = os.path.join(ROOT, 'conformance', 'rest-suite')
LOCK = os.path.join(ROOT, 'rest-suite.lock')
FRAGMENT = os.path.join(ROOT, 'stdlib', 'rest.schema-fragment.json')


def tree_digest(directory):
    rows = []
    for base, _dirs, files in os.walk(directory):
        for name in files:
            path = os.path.join(base, name)
            rel = os.path.relpath(path, directory).replace(os.sep, '/')
            with open(path, 'rb') as f:
                rows.append((rel.encode(), hashlib.sha256(f.read()).hexdigest()))
    rows.sort()
    return hashlib.sha256(b''.join(f'{h}  '.encode() + n + b'\n' for n, h in rows)).hexdigest()


def read_lock():
    with open(LOCK, encoding='utf-8') as f:
        return dict(line.rstrip('\n').split('=', 1) for line in f if '=' in line)


def main():
    if '--verify' in sys.argv[1:]:
        lock = read_lock()
        have = tree_digest(DEST)
        if have != lock.get('sha256'):
            sys.exit(f'conformance/rest-suite digest {have} differs from the lock {lock.get("sha256")}')
        with open(os.path.join(DEST, 'host.schema.json'), 'rb') as a, open(FRAGMENT, 'rb') as b:
            if a.read() != b.read():
                sys.exit('stdlib/rest.schema-fragment.json differs from conformance/rest-suite/host.schema.json')
        print(f'conformance/rest-suite matches {lock.get("commit")}')
        return
    spec = os.path.abspath(os.environ.get('LMCODE_SPEC_DIR') or os.path.join(ROOT, '..', 'lmcode'))
    commit = subprocess.check_output(['git', '-C', spec, 'rev-parse', 'HEAD'], text=True).strip()
    shutil.rmtree(DEST, ignore_errors=True)
    os.makedirs(os.path.dirname(DEST), exist_ok=True)
    shutil.copytree(os.path.join(spec, 'tests', 'rest-suite'), DEST, ignore=shutil.ignore_patterns('__pycache__'))
    shutil.copyfile(os.path.join(DEST, 'host.schema.json'), FRAGMENT)
    with open(LOCK, 'w', encoding='utf-8', newline='\n') as f:
        f.write(f'repo=https://github.com/lmctl-ai/lmcode\ncommit={commit}\nsha256={tree_digest(DEST)}\n')
    print(f'synced REST suite from lmcode {commit}')


main()
