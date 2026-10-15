"""The packaged smoke must opt in to the same developer scenarios as CTest."""
import re
import unittest

from package_smoke import ROOT, SMOKE_CASES, scenario_arguments


class PackageSmokeOptionsTest(unittest.TestCase):
    def test_smoke_flags_match_established_ctest_scenarios(self):
        cmake = (ROOT / 'native/targets/windows/scenarios/CMakeLists.txt').read_text(encoding='utf-8')
        for name, script, mode, ctest_name, developer in SMOKE_CASES:
            with self.subTest(case=name):
                declaration = re.search(r'uv_scenario\(' + re.escape(ctest_name) + r'(\s[^)]*|)\)', cmake)
                self.assertIsNotNone(declaration, ctest_name)
                expected = 'DEVELOPER' in declaration.group(1).split()
                arguments = scenario_arguments('game.exe', script, mode, developer)
                self.assertEqual('--developer' in arguments, expected, ctest_name)


if __name__ == '__main__':
    unittest.main()
