"""Compile and execute the actual controller with isolated native API doubles."""
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1]
WORKSPACE = MODULE.parents[1]


class SoloPlayTests(unittest.TestCase):
    def test_controller_behavior(self):
        cmake = os.environ.get('CMAKE') or shutil.which('cmake')
        self.assertTrue(cmake, 'Set CMAKE or add CMake to PATH for the native harness')
        (WORKSPACE / '.build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='solo-play-test-', dir=WORKSPACE / '.build') as temp:
            root = Path(temp)
            script = (MODULE / 'src/mod_solo_play.cpp').read_text()
            script = re.sub(r'^#include "(?!ScalingPolicy.h)[^"]+"\n', '', script, flags=re.M)
            (root / 'actual_script.cpp').write_text(script)
            core = WORKSPACE / '.build/managed-core/src/server/game/Entities/Unit'
            header = (core / 'Unit.h').read_text()
            cpp = (core / 'Unit.cpp').read_text()
            heal_class = header[header.index('class TC_GAME_API HealInfo'):header.index('class TC_GAME_API ProcEventInfo')]
            heal_impl = cpp[cpp.index('HealInfo::HealInfo('):cpp.index('uint32 HealInfo::GetHitMask(')]
            (root / 'actual_heal_info.h').write_text(heal_class.replace('TC_GAME_API ', '') + heal_impl)
            (root / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.18)\nproject(SoloPlayTest LANGUAGES CXX)\n'
                f'add_executable(behavior "{MODULE.as_posix()}/tests/behavior.cpp")\n'
                f'target_include_directories(behavior PRIVATE "{root.as_posix()}" "{MODULE.as_posix()}/src")\n'
                'target_compile_features(behavior PRIVATE cxx_std_20)\n')
            for cmd in ([cmake, '-S', temp, '-B', str(root / 'build')],
                        [cmake, '--build', str(root / 'build'), '--config', 'Release']):
                r = subprocess.run(cmd, capture_output=True, text=True)
                self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            exe = next((root / 'build').rglob('behavior.exe' if os.name == 'nt' else 'behavior'))
            r = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn('behavioral checks passed', r.stdout)
            print(r.stdout.strip())

    def test_donor_integrity(self):
        for file, expected in json.loads((MODULE / 'docs/donor-inputs.sha256.json').read_text()).items():
            self.assertEqual(hashlib.sha256((WORKSPACE / file).read_bytes()).hexdigest(), expected, file)

    def test_native_damage_bridge_dispatches_once_per_pipeline(self):
        core = WORKSPACE / '.build/managed-core/src/server/game'
        if not core.exists():
            self.skipTest('Run managed core preparation first')
        unit = (core / 'Entities/Unit/Unit.cpp').read_text()
        aura = (core / 'Spells/Auras/SpellAuraEffects.cpp').read_text()
        self.assertEqual(unit.count('sScriptMgr->OnScalingDamage('), 2)
        self.assertEqual(aura.count('sScriptMgr->OnScalingDamage('), 2)
        deal = unit[unit.index('uint32 Unit::DealDamage('):unit.index('void Unit::CalculateSpellDamageTaken(')]
        self.assertNotIn('OnScalingDamage', deal)
        self.assertEqual(unit.count('sScriptMgr->OnBeforeHeal('), 1)
        heal = unit[unit.index('void Unit::DealHeal('):unit.index('bool Unit::IsMagnet(')]
        self.assertNotIn('OnBeforeHeal(', heal)
        absorb = unit[unit.index('void Unit::CalcHealAbsorb('):]
        self.assertLess(absorb.index('healInfo.ApplyScaling();'), absorb.index('int32 const healing'))
        self.assertIn('heal = healInfo.GetHeal() + healInfo.GetAbsorb();', aura)

    def test_catalog_and_config(self):
        spec = importlib.util.spec_from_file_location('integration', WORKSPACE / 'tools/module-integration/integration.py')
        integration = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(integration)
        _, enabled = integration.discover(WORKSPACE / 'modules')
        own = next(m for m in enabled if m['id'] == 'mod-solo-play')
        self.assertEqual(own['entrypoint'], 'Addmod_solo_playScripts')
        config = (MODULE / 'conf/solo-play.conf.dist').read_text()
        self.assertRegex(config, r'(?m)^SoloPlay.Enable = 0$')
        self.assertTrue(all(k.startswith('SoloPlay.') for k in re.findall(r'^(\S+) =', config, re.M)))
        script = (MODULE / 'src/mod_solo_play.cpp').read_text()
        for key in re.findall(r'"(SoloPlay\.[^"]+)"', script):
            if not key.endswith('.'):
                self.assertIn(key + ' =', config)


if __name__ == '__main__':
    unittest.main()
