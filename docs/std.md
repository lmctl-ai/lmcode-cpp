# Built-in lmcode std v1

Canonical signatures: 72.

| Signature | Area | Semantics | Errors |
| --- | --- | --- | --- |
| `fn file_append(string, string) -> void` | file | Create or append all raw bytes to a regular file. | FUNCTION: PATH: OS reason |
| `fn file_exists(string) -> bool` | file | Return true only when metadata identifies a regular file; missing and nonregular paths return false. | FUNCTION: PATH: OS reason |
| `fn file_lines(string) -> StrList` | file | Read raw bytes and split on LF, preserving CR, empty fields, and the final field. | FUNCTION: PATH: OS reason |
| `fn file_read(string) -> string` | file | Read a complete regular file as raw bytes. | FUNCTION: PATH: OS reason |
| `fn file_write(string, string) -> void` | file | Create or truncate a regular file and write all raw bytes. | FUNCTION: PATH: OS reason |
| `fn float_list() -> FloatList` | collections | Construct an empty FloatList in the active collection arena. | — |
| `fn int_list() -> IntList` | collections | Construct an empty IntList in the active collection arena. | — |
| `fn math_abs(float) -> float` | math | Absolute value; integer arithmetic wraps, and float signed zero is cleared. | — |
| `fn math_abs(int) -> int` | math | Absolute value; integer arithmetic wraps, and float signed zero is cleared. | — |
| `fn math_ceil(float) -> float` | math | Identity for int; ceiling for binary64 while preserving signed zero. | — |
| `fn math_ceil(int) -> int` | math | Identity for int; ceiling for binary64 while preserving signed zero. | — |
| `fn math_floor(float) -> float` | math | Identity for int; floor for binary64 while preserving signed zero. | — |
| `fn math_floor(int) -> int` | math | Identity for int; floor for binary64 while preserving signed zero. | — |
| `fn math_max(float, float) -> float` | math | Maximum; integer comparison, or NaN propagation with first-operand ties for floats. | — |
| `fn math_max(int, int) -> int` | math | Maximum; integer comparison, or NaN propagation with first-operand ties for floats. | — |
| `fn math_min(float, float) -> float` | math | Minimum; integer comparison, or NaN propagation with first-operand ties for floats. | — |
| `fn math_min(int, int) -> int` | math | Minimum; integer comparison, or NaN propagation with first-operand ties for floats. | — |
| `fn math_pow(float, float) -> float` | math | Integer exponentiation wraps modulo 2^64; float power follows IEEE/libm special cases. | math_pow: integer exponent must be nonnegative |
| `fn math_pow(int, int) -> int` | math | Integer exponentiation wraps modulo 2^64; float power follows IEEE/libm special cases. | math_pow: integer exponent must be nonnegative |
| `fn math_round(float) -> float` | math | Identity for int; binary64 nearest integer with halfway cases away from zero. | — |
| `fn math_round(int) -> int` | math | Identity for int; binary64 nearest integer with halfway cases away from zero. | — |
| `fn math_sqrt(float) -> float` | math | Exact floor integer square root; binary64 square root for float. | math_sqrt: integer argument must be nonnegative |
| `fn math_sqrt(int) -> int` | math | Exact floor integer square root; binary64 square root for float. | math_sqrt: integer argument must be nonnegative |
| `fn str_contains(string, string) -> bool` | string | Byte substring search; an empty needle matches. | — |
| `fn str_ends_with(string, string) -> bool` | string | Byte suffix comparison; an empty suffix matches. | — |
| `fn str_find(string, string) -> int` | string | Return the first byte offset or -1; an empty needle returns 0. | — |
| `fn str_is_float(string) -> bool` | string | Report whether the complete decimal token is a finite binary64, including representable subnormals. | — |
| `fn str_is_int(string) -> bool` | string | Report whether the complete signed decimal token fits int64. | — |
| `fn str_len(string) -> int` | string | Return the raw byte length, including NUL and invalid UTF-8 bytes. | — |
| `fn str_list() -> StrList` | collections | Construct an empty StrList in the active collection arena. | — |
| `fn str_lower(string) -> string` | string | Lowercase ASCII A-Z bytes; preserve every other byte. | — |
| `fn str_map() -> StrMap` | collections | Construct an empty insertion-ordered StrMap in the active collection arena. | — |
| `fn str_match(string, string) -> bool` | string | Match a whole byte string with *, ?, classes, ranges, negation, and backslash escapes. | str_match: pattern exceeds the 2^27 work limit |
| `fn str_repeat(string, int) -> string` | string | Repeat raw bytes; reject negative counts and results above 64 MiB. | str_repeat: count must be nonnegative and result at most 67108864 bytes |
| `fn str_replace(string, string, string) -> string` | string | Replace nonoverlapping byte matches left to right; an empty search is a no-op. | str_replace: result must be at most 67108864 bytes |
| `fn str_split(string, string) -> StrList` | collections | Split raw bytes on every nonoverlapping nonempty separator into a StrList. | str_split: separator must not be empty |
| `fn str_starts_with(string, string) -> bool` | string | Byte prefix comparison; an empty prefix matches. | — |
| `fn str_substr(string, int, int) -> string` | string | Return a byte slice; clamp negative starts and overlong counts, with nonpositive counts empty. | — |
| `fn str_to_float(string) -> float` | string | Parse a finite locale-independent decimal binary64 token; return zero on failure. | — |
| `fn str_to_int(string) -> int` | string | Parse a complete signed decimal int64 token; return zero on failure. | — |
| `fn str_trim(string) -> string` | string | Remove ASCII space and bytes 0x09 through 0x0d from both ends. | — |
| `fn str_upper(string) -> string` | string | Uppercase ASCII a-z bytes; preserve every other byte. | — |
| `method FloatList.add(float) -> FloatList` | collections | Append a float and return the receiver. | — |
| `method FloatList.at(int) -> float` | collections | Return the element at a zero-based index. | FloatList.at: index out of range: i |
| `method FloatList.clear() -> FloatList` | collections | Remove elements and return the receiver. | — |
| `method FloatList.contains(float) -> bool` | collections | Test float == equality; NaN never matches. | — |
| `method FloatList.index_of(float) -> int` | collections | Return the first float == index or -1; NaN never matches. | — |
| `method FloatList.len() -> int` | collections | Return element count. | — |
| `method FloatList.set(int, float) -> FloatList` | collections | Replace an indexed float and return the receiver. | FloatList.set: index out of range: i |
| `method IntList.add(int) -> IntList` | collections | Append an int and return the receiver. | — |
| `method IntList.at(int) -> int` | collections | Return the element at a zero-based index. | IntList.at: index out of range: i |
| `method IntList.clear() -> IntList` | collections | Remove elements and return the receiver. | — |
| `method IntList.contains(int) -> bool` | collections | Test integer equality of elements. | — |
| `method IntList.index_of(int) -> int` | collections | Return the first equal index or -1. | — |
| `method IntList.len() -> int` | collections | Return element count. | — |
| `method IntList.set(int, int) -> IntList` | collections | Replace an indexed int and return the receiver. | IntList.set: index out of range: i |
| `method StrList.add(string) -> StrList` | collections | Append a byte string and return the receiver. | — |
| `method StrList.at(int) -> string` | collections | Return the element at a zero-based index. | StrList.at: index out of range: i |
| `method StrList.clear() -> StrList` | collections | Remove elements and return the receiver. | — |
| `method StrList.contains(string) -> bool` | collections | Test byte equality of elements. | — |
| `method StrList.index_of(string) -> int` | collections | Return the first byte-equal index or -1. | — |
| `method StrList.join(string) -> string` | collections | Join byte strings with a byte separator. | — |
| `method StrList.len() -> int` | collections | Return element count. | — |
| `method StrList.set(int, string) -> StrList` | collections | Replace an indexed byte string and return the receiver. | StrList.set: index out of range: i |
| `method StrMap.clear() -> StrMap` | collections | Remove entries and return the receiver. | — |
| `method StrMap.get(string) -> string` | collections | Return a byte-key value. | StrMap.get: missing key |
| `method StrMap.get_or(string, string) -> string` | collections | Return a byte-key value or the fallback. | — |
| `method StrMap.has(string) -> bool` | collections | Test byte-key presence. | — |
| `method StrMap.key_at(int) -> string` | collections | Return the insertion-order key at a zero-based index. | StrMap.key_at: index out of range: i |
| `method StrMap.len() -> int` | collections | Return insertion-order entry count. | — |
| `method StrMap.remove(string) -> bool` | collections | Remove a key and report whether it existed. | — |
| `method StrMap.set(string, string) -> StrMap` | collections | Insert or replace a value while preserving an existing key position. | StrMap.set: index out of range: i |
