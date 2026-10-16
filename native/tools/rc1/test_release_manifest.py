"""Regression cases for exact RC identity, including the independent review finding."""
import unittest

from release_manifest import verify_windows_build_info

COMMIT = '1234567890abcdef1234567890abcdef12345678'


class WindowsIdentityTest(unittest.TestCase):
    def info(self, version='0.5.0-rc1', source=None):
        return f'Version:      {version}\nSource:       {source or COMMIT + " (release/v0.5.0-rc1)"}\n'

    def test_exact_rc1(self):
        verify_windows_build_info(self.info(), COMMIT)

    def test_other_candidates_cannot_be_mislabeled_rc1(self):
        for version in ['0.5.0-rc10', '0.5.0-rc1-dev', '0.5.0', '0.5.0-rc1 extra']:
            with self.subTest(version=version), self.assertRaises(ValueError):
                verify_windows_build_info(self.info(version), COMMIT)

    def test_exact_rc2(self):
        verify_windows_build_info(self.info("0.5.0-rc2"), COMMIT, "0.5.0-rc2")

    def test_other_candidates_cannot_be_mislabeled_rc2(self):
        for version in ["0.5.0-rc1", "0.5.0-rc20", "0.5.0-rc2-dev", "0.5.0", "0.5.0-rc2 extra"]:
            with self.subTest(version=version), self.assertRaises(ValueError):
                verify_windows_build_info(self.info(version), COMMIT, "0.5.0-rc2")

    def test_dirty_source_is_rejected(self):
        with self.assertRaises(ValueError):
            verify_windows_build_info(self.info(source=COMMIT + ' (release/v0.5.0-rc1) + UNCOMMITTED CHANGES'), COMMIT)

    def test_wrong_commit_is_rejected(self):
        with self.assertRaises(ValueError):
            verify_windows_build_info(self.info(), '0' * 40)


if __name__ == '__main__':
    unittest.main()
