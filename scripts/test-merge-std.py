#!/usr/bin/env python3
"""Regression checks for the build-time std merge (standard library only)."""
import importlib.util
import json
from pathlib import Path
import unittest

root = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('merge_std', root / 'scripts/merge-std.py')
merge_std = importlib.util.module_from_spec(spec)
spec.loader.exec_module(merge_std)
standard = json.loads((root / 'std/lmcode-std.schema.json').read_text())


class MergeTests(unittest.TestCase):
    def host(self):
        return {'dsl.schema': 2, 'name': 'host', 'types': {}, 'functions': {}, 'context': {}, 'enums': {'State': ['READY']}}

    def test_canonical_matches_all_compatibility_fragments(self):
        types, functions = {}, {}
        for name in ['string', 'math', 'collections', 'testing']:
            fragment = json.loads((root / f'stdlib/{name}.schema-fragment.json').read_text())
            types.update(fragment.get('types', {}))
            functions.update(fragment.get('functions', {}))
        self.assertEqual(standard['types'], types)
        self.assertEqual(standard['functions'], functions)
        self.assertEqual(standard['lmcode.std'], 1)
        count = sum(len(overloads) for overloads in functions.values())
        count += sum(len(overloads) for item in types.values() for overloads in item['methods'].values())
        self.assertEqual(count, 69)

    def test_preserves_host_metadata_and_input(self):
        host = self.host()
        result = merge_std.merge_schema(host, standard)
        self.assertEqual(result['name'], 'host')
        self.assertEqual(result['enums'], host['enums'])
        self.assertEqual(host['functions'], {})
        self.assertEqual(result['types'], standard['types'])
        self.assertEqual(result['functions'], standard['functions'])

    def test_idempotent(self):
        once = merge_std.merge_schema(self.host(), standard)
        self.assertEqual(once, merge_std.merge_schema(once, standard))

    def test_compatible_partial_types_and_extra_overloads(self):
        host = self.host()
        host['types']['StrList'] = {'methods': {'len': [{'params': [], 'returns': 'int'}], 'host_method': [{'params': [], 'returns': 'void'}]}}
        host['functions']['math_abs'] = [{'params': ['string'], 'returns': 'string'}]
        result = merge_std.merge_schema(host, standard)
        self.assertIn('host_method', result['types']['StrList']['methods'])
        self.assertEqual(len(result['functions']['math_abs']), 3)

    def test_conflicting_function_rejected(self):
        host = self.host()
        host['functions']['str_len'] = [{'params': ['string'], 'returns': 'float'}]
        with self.assertRaisesRegex(ValueError, 'conflicting function str_len'):
            merge_std.merge_schema(host, standard)

    def test_conflicting_method_rejected(self):
        host = self.host()
        host['types']['StrList'] = {'methods': {'len': [{'params': [], 'returns': 'float'}]}}
        with self.assertRaisesRegex(ValueError, 'conflicting method StrList'):
            merge_std.merge_schema(host, standard)

    def test_schema_version_rejected(self):
        host = self.host()
        host['dsl.schema'] = 1
        with self.assertRaisesRegex(ValueError, 'dsl.schema 2'):
            merge_std.merge_schema(host, standard)


if __name__ == '__main__':
    unittest.main()
