"""Exercise the real artwork selector and Windows resource template, without copies."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class DesktopAssetLayoutTests(unittest.TestCase):
    def test_artwork_can_be_external_without_removing_imports(self):
        with tempfile.TemporaryDirectory(prefix='sfe-asset-layout-') as temporary:
            work = Path(temporary)
            for enabled in ('ON', 'OFF'):
                script = work / 'check.cmake'
                output = work / 'assets.rc'
                script.write_text(f'''
set(STARFOX_EMBED_ENHANCED_BACKDROPS {enabled})
include("{ROOT.as_posix()}/cmake/EnhancedBackdropAssets.cmake")
starfox_enhanced_backdrop_resources(arguments files)
list(LENGTH files count)
if(STARFOX_EMBED_ENHANCED_BACKDROPS)
    if(count LESS 38)
        message(FATAL_ERROR "Embedded artwork is missing")
    endif()
elseif(count OR arguments)
    message(FATAL_ERROR "External artwork was still embedded")
endif()
set(CMAKE_CURRENT_SOURCE_DIR "{ROOT.as_posix()}")
configure_file("{ROOT.as_posix()}/src/app/starfox_assets.rc.in" "{output.as_posix()}" @ONLY)
''', encoding='utf-8')
                subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True)
                text = output.read_text(encoding='utf-8')
                self.assertIn('IDR_STARFOX_PATCH RCDATA', text)
                self.assertIn('IDR_STARFOX_EX_SYMBOLS RCDATA', text)
                self.assertIn('IDR_RETAIL_GERMANY_V10_PATCH RCDATA', text)
                self.assertIn(('#define' if enabled == 'ON' else '/* #undef')
                              + ' STARFOX_EMBED_ENHANCED_BACKDROPS', text)
                compiler = shutil.which('c++') or shutil.which('gcc')
                if compiler:
                    # Test preprocessed payloads, not just an inactive text block.
                    result = subprocess.run([compiler, '-E', '-P', '-x', 'c', str(output)],
                                            check=True, capture_output=True, text=True)
                    self.assertEqual('200 RCDATA' in result.stdout, enabled == 'ON')
                    self.assertIn('101 RCDATA', result.stdout)
                    self.assertIn('126 RCDATA', result.stdout)

    def test_builder_uses_import_only_payload(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        block = cmake.split('set(STARFOX_IMPORT_ASSETS ', 1)[1].split('if(STARFOX_BUILD_VR)', 1)[0]
        self.assertNotIn('STARFOX_EMBED_BACKDROP', block)
        self.assertNotIn('starfox_embedded_assets', block)
        for identifier in (101, 102, 108, 109, *range(120, 127)):
            self.assertIn(f'--resource "{identifier}=', block)
        self.assertIn('"${STARFOX_IMPORT_ASSETS}")', block)


if __name__ == '__main__':
    unittest.main()
