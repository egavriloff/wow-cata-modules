"""Offline checks: no database connections or server startup."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1]
WORKSPACE = MODULE.parents[1]


class ModuleTests(unittest.TestCase):
    def test_cpp_behavior(self):
        cmake = os.environ.get('CMAKE') or shutil.which('cmake')
        self.assertTrue(cmake, 'CMake and C++ compiler are required')
        with tempfile.TemporaryDirectory(prefix='transmog-test-') as temp:
            source = Path(temp)
            script = (MODULE / 'src/npc_transmogrifier.cpp').read_text(encoding='utf-8')
            script = re.sub(r'^#include "(?!transmog_validation.h)[^"]+"', '', script, flags=re.M)
            (source / 'actual_script.cpp').write_text(script, encoding='utf-8')
            (source / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.18)\nproject(TransmogTest LANGUAGES CXX)\n'
                f'add_executable(behavior "{MODULE.as_posix()}/tests/behavior.cpp")\n'
                f'target_include_directories(behavior PRIVATE "{source.as_posix()}" "{MODULE.as_posix()}/src")\n'
                'target_compile_features(behavior PRIVATE cxx_std_20)\n'
                'if(MSVC)\ntarget_compile_options(behavior PRIVATE /utf-8)\nendif()\n', encoding='utf-8')
            for command in ([cmake, '-S', temp, '-B', str(source / 'build')],
                            [cmake, '--build', str(source / 'build'), '--config', 'Release']):
                result = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            binary = next((source / 'build').rglob('behavior.exe' if os.name == 'nt' else 'behavior'))
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('behavioral checks passed', result.stdout)
            print(result.stdout.strip())

    def test_catalog_registration_and_migrations(self):
        sys.path.insert(0, str(WORKSPACE / 'tools/sql-updater'))
        from catalog import discover
        from sql import statements
        from catalog import integration
        modules, enabled = integration.discover(WORKSPACE / 'modules')
        module = next(m for m in enabled if m['id'] == 'mod-npc-transmogrifier')
        self.assertEqual(module['entrypoint'], 'Addmod_npc_transmogrifierScripts')
        # Use the updater's actual discovery, SQL parser and normalized checksums.
        import inspect
        self.assertIn('workspace', inspect.signature(discover).parameters)
        migrations = discover(WORKSPACE)
        own = [m for m in migrations if m.source == 'module:mod-npc-transmogrifier']
        self.assertEqual([m.target for m in own], ['characters', 'world'])
        for migration in own:
            self.assertTrue(list(statements(migration.sql)))
            content = MODULE / 'sql' / migration.target / migration.relative
            normalized = content.read_bytes().replace(b'\r\n', b'\n')
            self.assertEqual(migration.checksum, hashlib.sha256(normalized).hexdigest())

    def test_native_backend_and_schema(self):
        script = (MODULE / 'src/npc_transmogrifier.cpp').read_text(encoding='utf-8')
        executable_script = re.sub(r'/\*.*?\*/|//[^\n]*', '', script, flags=re.S)
        for forbidden in ('ITEM_DYNAMIC_MODIFIERS', 'ITEM_FIELD_MODIFIERS_MASK',
                          'item_instance_transmog', 'GetDynamicUInt32Value', '_common/ModuleConfig.h'):
            self.assertNotIn(forbidden, executable_script)
        item = (WORKSPACE / 'externals/core/src/server/game/Entities/Item/Item.h').read_text(encoding='utf-8')
        self.assertIn('GetEnchantmentId(TRANSMOGRIFY_ENCHANTMENT_SLOT)', item)
        characters = (MODULE / 'sql/characters/001_sets.sql').read_text(encoding='utf-8')
        self.assertNotIn('CREATE TABLE IF NOT EXISTS `custom_transmogrification`', characters)
        self.assertIn('`Owner` INT UNSIGNED', characters)
        native = (WORKSPACE / 'externals/core/sql/base/characters_database.sql').read_text(encoding='utf-8')
        self.assertRegex(native, r'`enchantments`\s+text')
        world = (MODULE / 'sql/world/001_npc.sql').read_text(encoding='utf-8')
        executable = re.sub(r'--[^\n]*', '', world)
        self.assertNotIn('dynamicflags', executable)
        self.assertIn('`creature_template_model`', executable)
        self.assertIn('`npcflag` = NULL', executable)
        self.assertNotIn('INSERT INTO `creature`', executable)
        self.assertIn("'npc_transmogrifier'", executable)
        models = (WORKSPACE / 'externals/core/sql/updates/world/4.3.4/2023_09_15_00_world.sql').read_text(encoding='utf-8')
        self.assertIn('(29261, 0, 26078, 1, 15595)', models)

    def test_utf8(self):
        for file in MODULE.rglob('*'):
            if file.suffix in ('.cpp', '.h', '.md', '.sql', '.json', '.dist'):
                text = file.read_text(encoding='utf-8-sig')
                self.assertNotIn('\ufffd', text, str(file))
                self.assertNotIn('РќР°', text, str(file))
        script = (MODULE / 'src/npc_transmogrifier.cpp').read_text(encoding='utf-8')
        self.assertIn('"Назад"', script)
        self.assertIn('"Закрыть"', script)


if __name__ == '__main__':
    unittest.main()
