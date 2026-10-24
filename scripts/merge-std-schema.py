#!/usr/bin/env python3
"""Compatibility entry point for scripts/merge-std.py."""
from pathlib import Path
import importlib.util

_spec = importlib.util.spec_from_file_location("merge_std", Path(__file__).with_name("merge-std.py"))
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)
globals().update({key: value for key, value in vars(_module).items() if not key.startswith("__")})

if __name__ == "__main__":
    main()
