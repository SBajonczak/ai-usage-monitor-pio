"""Offline regression tests for the PlatformIO/SCons '__file__' failure."""
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts' / 'fetch_waveshare.py'
FILES = (
    'amoled.cpp', 'amoled.h', 'FT3168.cpp', 'FT3168.h', 'board_config.h',
    'low_level_amoled.c', 'low_level_amoled.h', 'i2c.c', 'i2c.h'
)


class DummyPioEnv:
    def __init__(self, project_dir):
        self.project_dir = str(project_dir)

    def subst(self, arg):
        assert arg == '$PROJECT_DIR'
        return self.project_dir


class FetchTests(unittest.TestCase):
    def test_scons_execution_without___file__(self):
        """Matches PlatformIO: exec compiled script with Import/env, no __file__."""
        with TemporaryDirectory() as tmp:
            dest = Path(tmp) / 'src' / 'vendor'
            dest.mkdir(parents=True)
            for filename in FILES:
                (dest / filename).write_text('dummy driver for offline test')
            namespace = {'__name__': '__scons__'}

            def mock_import(symbol):
                self.assertEqual(symbol, 'env')
                namespace['env'] = DummyPioEnv(tmp)

            namespace['Import'] = mock_import
            self.assertNotIn('__file__', namespace)
            exec(compile(SCRIPT.read_text(encoding='utf-8'), str(SCRIPT), 'exec'), namespace)
            self.assertEqual(namespace['project_root'](), Path(tmp).resolve())

    def test_standalone_invocation_with___file__(self):
        """Path resolution also works with plain python, without SCons Import."""
        namespace = {'__name__': '__standalone__', '__file__': str(SCRIPT)}
        # Avoid download attempts by explicitly limiting ensure_drivers to a
        # populated temporary target and bypassing auto-run only for this test.
        source = SCRIPT.read_text(encoding='utf-8')
        source = source.replace('\nensure_drivers()\n', '\n# no automatic bootstrap in unit test\n')
        exec(compile(source, str(SCRIPT), 'exec'), namespace)
        self.assertEqual(namespace['project_root'](), ROOT)


if __name__ == '__main__':
    unittest.main()
