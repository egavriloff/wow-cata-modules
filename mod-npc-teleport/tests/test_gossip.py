"""Compile real gossip code with small test doubles, independently of core libraries."""
import os
import ast
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADERS = ("ScriptMgr.h", "PassiveAI.h", "Chat.h", "Creature.h", "DBCStores.h", "Map.h",
           "MapManager.h", "Player.h", "ScriptedGossip.h", "TerrainMgr.h", "Define.h", "Log.h", "ObjectMgr.h")


class GossipTests(unittest.TestCase):
    def test_actual_script_with_adversarial_inputs(self):
        cmake = os.environ.get("CMAKE") or shutil.which("cmake")
        if not cmake:
            self.skipTest("CMake/compiler required for C++ gossip harness")
        with tempfile.TemporaryDirectory(prefix="npc-teleport-test-") as temp:
            source = Path(temp) / "source"
            source.mkdir()
            for header in HEADERS:
                (source / header).write_text('#include "gossip_stubs.h"\n', encoding="utf-8")
            harness = (ROOT / "tests/gossip_harness.cpp").as_posix()
            stubs = (ROOT / "tests").as_posix()
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.18)\nproject(GossipTest LANGUAGES CXX)\n'
                f'add_executable(gossip-test "{harness}")\n'
                f'target_include_directories(gossip-test PRIVATE "{source.as_posix()}" "{stubs}")\n'
                'target_compile_features(gossip-test PRIVATE cxx_std_20)\n'
                f'add_executable(utf8-probe "{stubs}/utf8_probe.cpp" "{ROOT.as_posix()}/src/teleport_data.cpp")\n'
                f'target_include_directories(utf8-probe PRIVATE "{source.as_posix()}" "{stubs}")\n'
                'target_compile_features(utf8-probe PRIVATE cxx_std_20)\n'
                'if(MSVC)\n  target_compile_options(gossip-test PRIVATE /utf-8)\nendif()\n'
                'if(MSVC)\n  target_compile_options(utf8-probe PRIVATE /utf-8)\nendif()\n'
                'enable_testing()\nadd_test(NAME gossip COMMAND gossip-test)\n', encoding="utf-8")
            build = Path(temp) / "build"
            for command in ([cmake, "-S", str(source), "-B", str(build)],
                            [cmake, "--build", str(build), "--config", "Release"],
                            [str(Path(cmake).with_name("ctest.exe" if os.name == "nt" else "ctest")),
                             "--test-dir", str(build), "-C", "Release", "--output-on-failure", "-V"]):
                result = subprocess.run(command, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                if "--test-dir" in command:
                    self.assertIn("28 gossip/payment/registration scenarios passed", result.stdout)
            probe = next(build.rglob("utf8-probe.exe" if os.name == "nt" else "utf8-probe"))
            data = subprocess.check_output([str(probe)]).decode("utf-8", errors="strict").split("\0")[:-1]
            from test_destinations import read_cpp
            expected = [r["name"].replace("\\'", "'") for r in read_cpp(ROOT / "src/teleport_data.cpp")]
            expected += [ast.literal_eval(m[1]) for m in re.finditer(
                r'\[\] = (".*?");', (ROOT / "src/teleport_text.h").read_text(encoding="utf-8"))]
            self.assertEqual(data, expected)


if __name__ == "__main__":
    unittest.main()
