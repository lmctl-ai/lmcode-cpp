#!/usr/bin/env python3
"""Build a schema-2 host schema with the canonical lmcode std declarations."""
import argparse
import copy
import json
from pathlib import Path
import sys


def merge_overloads(host, standard, label):
    for name, declarations in standard.items():
        existing = host.setdefault(name, [])
        for declaration in declarations:
            same_parameters = [entry for entry in existing if entry['params'] == declaration['params']]
            if same_parameters:
                if len(same_parameters) != 1 or any(same_parameters[0].get(key) != value for key, value in declaration.items()):
                    raise ValueError(f'conflicting {label} {name}{tuple(declaration["params"])}')
            else:
                existing.append(copy.deepcopy(declaration))


def merge_schema(host, standard):
    if host.get('dsl.schema') != 2 or standard.get('dsl.schema') != 2:
        raise ValueError('host and std schemas must use dsl.schema 2')
    result = copy.deepcopy(host)
    merge_overloads(result.setdefault('functions', {}), standard['functions'], 'function')
    types = result.setdefault('types', {})
    for name, definition in standard['types'].items():
        if name not in types:
            types[name] = copy.deepcopy(definition)
            continue
        current = types[name]
        for key, value in definition.items():
            if key == 'methods':
                merge_overloads(current.setdefault('methods', {}), value, f'method {name}.')
            elif key in current and current[key] != value:
                raise ValueError(f'conflicting type {name}.{key}')
            else:
                current[key] = copy.deepcopy(value)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host', help='host schema JSON, or - for stdin')
    parser.add_argument('output', help='merged schema JSON, or - for stdout')
    parser.add_argument('--std', type=Path, default=Path(__file__).resolve().parent.parent / 'std/lmcode-std.schema.json')
    args = parser.parse_args()
    try:
        host = json.loads(sys.stdin.read() if args.host == '-' else Path(args.host).read_text(encoding='utf-8'))
        standard = json.loads(args.std.read_text(encoding='utf-8'))
        result = json.dumps(merge_schema(host, standard), ensure_ascii=False, indent=2) + '\n'
        if args.output == '-':
            sys.stdout.write(result)
        else:
            Path(args.output).write_text(result, encoding='utf-8', newline='\n')
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(2, f'merge-std: {error}\n')


if __name__ == '__main__':
    main()
