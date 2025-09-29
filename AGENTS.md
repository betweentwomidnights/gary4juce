# AGENTS.md — Sanitize Non-ASCII UI Strings in JUCE (Limited Scope)

## Goal (Very Limited Scope)
Remove or safely normalize **non-ASCII characters** from **runtime UI string literals** in our JUCE plugin so Windows builds no longer trip the `juce::String(const char*)` / shaped-text assertions. **Do not change functionality** beyond the cosmetic substitution of glyphs. **Do not** touch audio/DSP/backend logic.

Target files (primary focus):
- `PluginEditor.cpp`  ← **behemoth; main target**
- (Secondary if needed) `DariusUI.cpp`, `DariusUI.h`

> We **do not** want a new helper function. Prefer plain ASCII. Only if a symbol is essential (e.g., a triangle), use an explicit UTF-8 constructor inline at the call site.

---

## Background
- JUCE asserts when constructing `juce::String` from 8-bit literals that contain bytes > 127, because encoding is ambiguous (ASCII vs. UTF-8 vs. codepage).
- We previously saw crashes with: ellipsis `…`, en/em dashes `– —`, triangles `▸ ▾`, smart quotes `“ ” ‘ ’`, etc.
- We’ve fixed some spots; we now want a **complete, repeatable sweep** of `PluginEditor.cpp` (and optionally `DariusUI.*`) to ensure there are **no non-ASCII glyphs in string literals** that are used at runtime.

---

## High-Level Plan
1. **Detect** any non-ASCII bytes **inside C/C++ string literals**.
2. **Replace** them with safe ASCII equivalents (preferred), or (only when essential) with `juce::String(juce::CharPointer_UTF8(u8"..."))` inline at the call site.
3. **Verify** no runtime strings contain non-ASCII; produce a diff.
4. **Build & smoke test** (compile only) to ensure zero functional changes beyond text.

---

## Branch / Safeguards
- Create a branch: `fix/non-ascii-ui-strings`.
- Work only under that branch. Do not change other files.
- Commit in small, logically grouped chunks (detection → replacement → verification).

```bash
git checkout -b fix/non-ascii-ui-strings
```

---

## What to Replace (Mapping Table)

Prefer ASCII. Use these canonical mappings:

| Glyph | Unicode | Replace with (ASCII) | Notes |
|------:|:-------:|:---------------------|:------|
| `…`   | U+2026  | `...`                | Ellipsis → three dots |
| `–`   | U+2013  | `-`                  | En dash |
| `—`   | U+2014  | `--`                 | Em dash → double hyphen |
| `“`   | U+201C  | `"`                  | Left double quote |
| `”`   | U+201D  | `"`                  | Right double quote |
| `‘`   | U+2018  | `'`                  | Left single quote |
| `’`   | U+2019  | `'`                  | Right single quote / apostrophe |
| `×`   | U+00D7  | `x`                  | Multiply sign |
| `•`   | U+2022  | `*`                  | Bullet |
| `°`   | U+00B0  | ` deg`               | Degree symbol |
| `→`   | U+2192  | `->`                 | Arrow |
| `←`   | U+2190  | `<-`                 | Arrow |
| `▲`   | U+25B2  | `^`                  | Triangle up |
| `▼`   | U+25BC  | `v`                  | Triangle down |
| `▸`   | U+25B8  | `>`                  | Triangle right |
| `▾`   | U+25BE  | `v`                  | Triangle down small |
| any other non-ASCII | (various) | replace with closest ASCII | If truly essential, see “UTF-8 inline” below |

> Ranges like `0.00–1.00` must become `0.00-1.00`.

**UTF-8 inline (only if symbol must be preserved):**
```cpp
// Example: keep a right triangle symbol at a single call site
someButton.setButtonText(juce::String(juce::CharPointer_UTF8(u8"\u25B8"))); // ▸
```

Do **not** introduce a global helper. Inline the explicit constructor only where strictly necessary.

---

## Detection Strategy

We want to flag **non-ASCII characters inside string literals** (i.e., `"..."` or `u8"..."`). A quick coarse pass:

### Option A: Python detector (preferred, single file pass)
Runs on `PluginEditor.cpp` and prints every offending literal with context and byte/line positions, plus a suggested ASCII replacement from the mapping table.

```python
# tools/find_non_ascii_strings.py
import re, sys, pathlib

mapping = {
    "…":"...","–":"-","—":"--","“":'"',"”":'"',"‘":"'", "’":"'",
    "×":"x","•":"*","°":" deg","→":"->","←":"<-",
    "▲":"^","▼":"v","▸":">","▾":"v",
}

str_pat = re.compile(r'("([^"\\]|\\.)*")')  # naive C string literal matcher (OK for our file)
path = pathlib.Path(sys.argv[1])
txt  = path.read_text(encoding="utf-8", errors="ignore")

for i, m in enumerate(str_pat.finditer(txt), 1):
    s = m.group(1)
    # find non-ascii
    bad = [ch for ch in s if ord(ch) > 127]
    if bad:
        line = txt.count("\n", 0, m.start()) + 1
        print(f"\nLine {line}: {s}")
        uniq = sorted(set(bad))
        print("  Non-ASCII:", uniq)
        print("  Suggestions:", {ch: mapping.get(ch, "<decide>") for ch in uniq})
```

Run:
```bash
python tools/find_non_ascii_strings.py PluginEditor.cpp
```

### Option B: ripgrep (quick coarse check)
```bash
# highlight any byte > 0x7F
rg -n --passthru "[^\x00-\x7F]" PluginEditor.cpp
```

> Use A for precise string-literal hits; B will also flag comments. We mainly care about **literals that feed JUCE text APIs**.

---

## Replacement Workflow

1. **Manual, targeted edits** in `PluginEditor.cpp`:
   - Replace per the table **inside the string literals**.
   - Where a symbol is essential and has no decent ASCII (rare), switch the entire argument to:
     ```cpp
     someLabel.setText(juce::String(juce::CharPointer_UTF8(u8"...")), juce::dontSendNotification);
     ```

2. **Do not** change variable names, code logic, or anything but string content.
3. Keep punctuation/spacing sane after substitution.

Commit:
```bash
git add PluginEditor.cpp
git commit -m "Sanitize non-ASCII UI literals in PluginEditor.cpp (ASCII or UTF-8 inline)"
```

Optional: repeat on `DariusUI.cpp` / `.h` if detector flags anything there.

---

## Verification

1. **Detector must be clean**:
```bash
python tools/find_non_ascii_strings.py PluginEditor.cpp
rg -n --passthru "[^\x00-\x7F]" PluginEditor.cpp | rg -v '^\s*//'
```

2. **Build** (no warnings/errors from JUCE string asserts at runtime):
- Windows: open solution / build, or
```powershell
msbuild /m /p:Configuration=Release
```

3. **Runtime smoke** (manual):
- Click **Generate** multiple times
- Toggle any buttons/labels that previously used special glyphs (triangles, dashes, ranges, ellipses)
- Confirm no shaped-text assertions

4. **Diff** (ensure limited blast radius):
```bash
git --no-pager diff --word-diff=plain HEAD~1
```

---

## Out-of-Scope (do NOT change)
- Audio/DSP processing
- Backend/networking logic
- Any function signatures or behavior
- Any non-UI files
- Comments are fine to leave as-is (but removing Unicode in comments is harmless)

---

## Deliverables
- A single PR/commit on `fix/non-ascii-ui-strings` updating **only** `PluginEditor.cpp` (and optionally `DariusUI.*` if flagged by detector).
- Detector output **before/after** (in PR description).
- Short note listing any spots where UTF-8 inline was used instead of ASCII (should be minimal).

---

## Gotchas & Notes
- Some fonts don’t contain certain glyphs even if encoded as UTF-8; that’s another reason we prefer ASCII where possible.
- If a string concatenates variables and literals, ensure you replace only the literal part.
- If you see `StringRef`/`TRANS`/`translate()` calls, still treat their literals the same way (ASCII or explicit UTF-8).

---

## Contact
If any ambiguity arises (e.g., symbol choice), pick the closest ASCII and leave a code comment `// ascii: was U+2014` for future design polish. No behavioral changes allowed.
