import importlib.util
from pathlib import Path
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import generate_ios_assets

spec = importlib.util.spec_from_file_location('package_ios', Path(__file__).resolve().parents[1] / 'tools/package_ios.py')
ios = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ios)


class IpaTests(unittest.TestCase):
    def app(self, root, platform=2):
        app = root / 'StarFoxEnhanced.app'
        app.mkdir()
        info = {'CFBundleExecutable': 'StarFoxEnhanced', 'CFBundleIdentifier': 'com.starfox.enhanced',
                'CFBundleSupportedPlatforms': ['iPhoneOS'], 'MinimumOSVersion': '15.0'}
        info['CFBundleIcons'] = {'CFBundlePrimaryIcon': {'CFBundleIconFiles': ['AppIcon60']}}
        info['CFBundleIcons~ipad'] = {'CFBundlePrimaryIcon': {
            'CFBundleIconFiles': ['AppIcon60', 'AppIcon76', 'AppIcon83.5']}}
        (app / 'Info.plist').write_bytes(plistlib.dumps(info))
        generate_ios_assets.generate(app)
        binary = struct.pack('<8I', 0xfeedfacf, 0x100000c, 0, 2, 1, 24, 0, 0)
        binary += struct.pack('<6I', 0x32, 24, platform, 0, 0, 0)
        (app / 'StarFoxEnhanced').write_bytes(binary)
        (app / 'StarFoxEnhanced').chmod(0o755)
        return app

    def test_direct_app_is_rooted_ipa(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = ios.package(root / 'test.ipa', app=self.app(root))
            self.assertEqual(result['app'], 'Payload/StarFoxEnhanced.app')
            self.assertEqual(ios.verify(root / 'test.ipa'), result)

    def test_old_wrapped_release_repaired_without_payload_change(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app = self.app(root)
            with zipfile.ZipFile(root / 'old.zip', 'w') as source:
                for path in app.iterdir():
                    entry = zipfile.ZipInfo('release/Payload/StarFoxEnhanced.app/' + path.name)
                    entry.create_system = 3
                    entry.external_attr = 0o100755 << 16
                    source.writestr(entry, path.read_bytes())
                source.writestr('release/README.md', 'Not inside the IPA')
            with self.assertRaisesRegex(ValueError, 'root'):
                ios.verify(root / 'old.zip')
            ios.package(root / 'test.ipa', source=root / 'old.zip')
            with zipfile.ZipFile(root / 'test.ipa') as repaired:
                self.assertEqual(repaired.read('Payload/StarFoxEnhanced.app/StarFoxEnhanced'), (app / 'StarFoxEnhanced').read_bytes())
                self.assertTrue(all(name.startswith('Payload/') for name in repaired.namelist()))
                self.assertEqual(repaired.getinfo('Payload/StarFoxEnhanced.app/StarFoxEnhanced').external_attr >> 16 & 0o777, 0o755)

    def test_simulator_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, 'physical iOS'):
                ios.package(root / 'bad.ipa', app=self.app(root, platform=7))
            self.assertFalse((root / 'bad.ipa').exists())

    def test_no_accidental_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / 'existing.ipa'
            output.write_bytes(b'keep')
            with self.assertRaisesRegex(ValueError, 'overwrite'):
                ios.package(output, app=self.app(root))
            self.assertEqual(output.read_bytes(), b'keep')

    def test_path_escape_rejected(self):
        for name in ('../Payload/app', '/Payload/app', 'Payload/../../app', 'Payload\\app'):
            with self.assertRaises(ValueError):
                ios.safe_name(name)

    def test_missing_icon_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app = self.app(root)
            (app / 'AppIcon83.5@2x.png').unlink()
            with self.assertRaises(KeyError):
                ios.package(root / 'bad.ipa', app=app)
            self.assertFalse((root / 'bad.ipa').exists())

    def test_missing_ipad_declaration_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app = self.app(root)
            info = plistlib.loads((app / 'Info.plist').read_bytes())
            del info['CFBundleIcons~ipad']
            (app / 'Info.plist').write_bytes(plistlib.dumps(info))
            with self.assertRaisesRegex(ValueError, 'icon declaration'):
                ios.package(root / 'bad.ipa', app=app)

    def test_corrupt_or_wrong_size_icon_rejected(self):
        for corrupt in (True, False):
            with self.subTest(corrupt=corrupt), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                app = self.app(root)
                icon = app / 'AppIcon60@3x.png'
                data = bytearray(icon.read_bytes() if corrupt else (app / 'AppIcon60@2x.png').read_bytes())
                if corrupt:
                    data[-5] ^= 1
                icon.write_bytes(data)
                with self.assertRaisesRegex(ValueError, 'PNG'):
                    ios.package(root / 'bad.ipa', app=app)

    @unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake/Ninja unavailable')
    def test_cmake_generates_bundle_resources_and_icon_declarations(self):
        repo = Path(__file__).resolve().parents[1].as_posix()
        with tempfile.TemporaryDirectory(prefix='ios icon probe ') as directory:
            root = Path(directory)
            # Exercise the production helper, including generated dependencies
            # and resource locations. No Apple compiler or executable copy.
            (root / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.21)
project(IosIconProbe NONE)
include("{repo}/cmake/IosIcons.cmake")
add_custom_target(icon_probe)
starfox_add_ios_icons(icon_probe)
get_target_property(icons icon_probe SOURCES)
get_target_property(compress icon_probe XCODE_ATTRIBUTE_COMPRESS_PNG_FILES)
get_target_property(combine icon_probe XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES)
if(NOT compress STREQUAL "NO" OR NOT combine STREQUAL "NO")
  message(FATAL_ERROR "Icon PNGs must remain standard RGB PNGs")
endif()
foreach(icon IN LISTS icons)
  get_source_file_property(location "${{icon}}" MACOSX_PACKAGE_LOCATION)
  if(NOT location STREQUAL "Resources")
    message(FATAL_ERROR "Icon is not a bundle resource")
  endif()
endforeach()
add_custom_target(generate_icons ALL DEPENDS ${{icons}})
configure_file("{repo}/platform/apple/Info.plist.in" Info.plist)
''')
            for command in (['cmake', '-S', str(root), '-B', str(root / 'build'), '-G', 'Ninja'],
                            ['cmake', '--build', str(root / 'build')]):
                result = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            info = plistlib.loads((root / 'build/Info.plist').read_bytes())
            self.assertIn('AppIcon60', info['CFBundleIcons']['CFBundlePrimaryIcon']['CFBundleIconFiles'])
            self.assertIn('AppIcon83.5', info['CFBundleIcons~ipad']['CFBundlePrimaryIcon']['CFBundleIconFiles'])
            self.assertEqual({p.name for p in (root / 'build/generated/ios-icons').iterdir()},
                             set(ios.ICON_SIZES))


if __name__ == '__main__':
    unittest.main()
