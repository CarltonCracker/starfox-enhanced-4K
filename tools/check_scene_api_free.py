"""Guard shared scene API isolation and its older Sniper-library compatibility."""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
forbidden = re.compile(r'(^|/)(vulkan|openxr|SDL3?|d3d|dxgi|Metal)[^/]*(/|\.h|$)|vulkan_|openxr_', re.IGNORECASE)
include = re.compile(rb'^\s*#\s*include\s*([<"])([^>"\r\n]+)[>"]', re.MULTILINE)
direct_bit_cast = re.compile(rb'\bstd\s*::\s*bit_cast\s*<')
bit_cast_wrapper = (root / 'include/starfox/compat/bit_cast.hpp').resolve()
cmake = (root / 'cmake/Scene.cmake').read_text(encoding='utf-8')
library = cmake[cmake.index('add_library(starfox_scene'):]
sources = re.findall(r'src/[\w/]+\.cpp', library[:library.index(')')])
if not sources:
    sys.exit('No starfox_scene sources found in cmake/Scene.cmake')


def resolve(name, current):
    for base in (root / 'include', current.parent):
        candidate = (base / name).resolve()
        if candidate.is_file():
            return candidate
    return None


failures = []
compatibility_failures = set()
for source in sources:
    seen = set()
    stack = [((root / source).resolve(), (source,))]
    while stack:
        path, chain = stack.pop()
        if path in seen:
            continue
        seen.add(path)
        source_text = path.read_bytes()
        # Sniper's GCC 10 library has <bit>, but not std::bit_cast. The wrapper
        # alone may select that library call; shared scene code must use it.
        if path != bit_cast_wrapper and direct_bit_cast.search(source_text):
            compatibility_failures.add(path.relative_to(root).as_posix())
        for _, name in include.findall(source_text):
            name = name.decode('utf-8')
            if forbidden.search(name):
                failures.append(' -> '.join(chain + (name,)))
                continue
            resolved = resolve(name, path)
            if resolved is not None:
                stack.append((resolved, chain + (resolved.relative_to(root).as_posix(),)))
for failure in failures:
    print('graphics API header reachable from starfox_scene:', failure)
for failure in sorted(compatibility_failures):
    print('Sniper-incompatible std::bit_cast; use starfox::bit_cast:', failure)
print(f'{len(sources)} starfox_scene sources checked')
sys.exit(1 if failures or compatibility_failures else 0)
