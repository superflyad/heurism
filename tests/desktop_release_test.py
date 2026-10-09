"""Root-owned isolated release fixtures; never alter installed desktop links."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, '/opt/companion/desktop')
import release


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        self.root = self.base/'releases'
        self.root.mkdir()
        self.link = self.base/'desktop'
        self.previous = self.base/'previous'
        self.patches = [patch.object(release, 'ROOT', self.root), patch.object(release, 'LINK', self.link),
                        patch.object(release, 'PREVIOUS', self.previous)]
        for item in self.patches:
            item.start()
        self.good = self.make('good')
        self.new = self.make('new')
        self.link.symlink_to(self.new)
        self.previous.write_text(str(self.good))

    def make(self, name):
        path = self.root/name
        path.mkdir()
        for filename in release.REQUIRED:
            (path/filename).write_text('Public release fixture\n')
            (path/filename).chmod(0o644)
        release.seal(path)
        return path

    def tearDown(self):
        for item in reversed(self.patches):
            item.stop()
        self.temp.cleanup()

    def test_integrity_and_corruption(self):
        self.assertEqual(release.verify(self.new), self.new)
        (self.new/'shell.py').write_text('Modified fixture')
        with self.assertRaises(ValueError):
            release.verify(self.new)

    def test_empty_manifest_rejected(self):
        (self.new/'hashes.json').write_text('{}')
        with self.assertRaises(ValueError):
            release.verify(self.new)

    def test_user_writable_code_rejected(self):
        (self.new/'shell.py').chmod(0o666)
        with self.assertRaises(ValueError):
            release.verify(self.new)

    def test_failed_candidate_rolls_back_without_needing_its_integrity(self):
        (self.new/'shell.py').write_text('Broken fixture')
        release.rollback()
        self.assertEqual(self.link.resolve(), self.good)
        self.assertEqual(self.previous.read_text().strip(), str(self.new))
        self.assertEqual(release.verify(self.link), self.good)

    def test_broken_fallback_does_not_change_active_link(self):
        (self.good/'shell.py').write_text('Broken fallback')
        with self.assertRaises(ValueError):
            release.rollback()
        self.assertEqual(self.link.resolve(), self.new)


if __name__ == '__main__':
    unittest.main()
