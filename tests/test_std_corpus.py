#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('corpus',ROOT/'scripts/gen-std-corpus.py')
corpus=importlib.util.module_from_spec(spec);spec.loader.exec_module(corpus)
class CorpusTests(unittest.TestCase):
    def test_reproducible(self):
        self.assertEqual(corpus.cases([42],4),corpus.cases([42],4))
        self.assertNotEqual(corpus.cases([42],4),corpus.cases([43],4))
    def test_byte_globs(self):
        for text,pattern,want in [(b'\x00\xff',b'??',True),(b'*',b'\\*',True),(b'z',b'[z-a]',False),(b'[',b'[',True),(b']',b'[]a]',True),(b'\xff',b'[!a-z]',True)]:
            self.assertEqual(corpus.glob(text,pattern),want)
    def test_numeric_boundaries(self):
        self.assertEqual(corpus.numeric(b'9223372036854775807')[:2],(True,corpus.I64_MAX))
        self.assertEqual(corpus.numeric(b'9223372036854775808')[:2],(False,0))
        self.assertFalse(corpus.numeric(b'1\x00')[2]);self.assertFalse(corpus.numeric(b'1e-999')[2]);self.assertTrue(corpus.numeric(b'5e-324')[2])
    def test_stale_source_detected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);corpus.generate(root,[7],1)
            corpus.generate(root,[7],1,check=True)
            (root/'fuzz-0000000007.dsl').write_text('t.int(0);\n')
            with self.assertRaises(SystemExit):corpus.generate(root,[7],1,check=True)
if __name__=='__main__': unittest.main()
