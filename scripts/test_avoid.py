#!/usr/bin/env python3
"""Build and run production Avoid with real client prediction physics (MSVC Debug)."""

import argparse
import os
import shutil
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parent.parent
os.chdir(root)
parser = argparse.ArgumentParser(description="Run Avoid regression tests against an MSVC Debug game-client build")
parser.add_argument("--build-dir", default="build-spinbot", type=Path)
build = parser.parse_args().build_dir.resolve()
cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
instance = next(line.split("=", 1)[1] for line in cache.splitlines() if line.startswith("CMAKE_GENERATOR_INSTANCE:"))
vc = str(Path(instance) / "VC/Auxiliary/Build/vcvars64.bat")
r = subprocess.run(f'cmd /d /s /c ""{vc}" >nul && set"',
                   capture_output=True, text=True, check=True)
env = dict(line.split('=', 1) for line in r.stdout.splitlines()
           if '=' in line and not line.startswith('='))
cl = shutil.which("cl.exe", path=next(value for key, value in env.items() if key.lower() == "path"))
if cl is None:
    raise RuntimeError("MSVC compiler was not found after vcvars64.bat")
s = (root / 'src/game/client/gameclient.cpp').read_text(encoding='utf-8')
helpers = s[s.index('namespace\n{\n\tbool DmdIsFreezeMapIndex'):s.index('\nvoid CGameClient::ConZzFentBotSavePlrStart')]
method = s[s.index('bool CGameClient::ApplyAvoid('):s.index('\nvoid CGameClient::ResetKinetixLaserUnfreeze')]
(build / 'avoid_implementation.inc').write_text(helpers + '\n' + method, encoding='utf-8')
objs = ['character', 'door', 'dragger', 'laser', 'pickup', 'plasma',
        'projectile', 'entity', 'gameworld', 'laser_data', 'pickup_data',
        'projectile_data', 'client_data']
args = [cl, "/nologo", "/std:c++20", "/EHsc", "/MTd", "/Zi", "/utf-8",
        "/D_DEBUG", "/DCONF_DEBUG", "/DNOMINMAX", f"/I{build / 'src'}",
        f"/I{build}", "/Isrc", "src/test/avoid_regression.cpp",
        f"/Fe{build / 'avoid_test.exe'}", f"/Fo{build / 'avoid_test.obj'}",
        f"/Fd{build / 'avoid_test.pdb'}"]
args += [str(build / "game-client.dir/Debug" / (name + ".obj")) for name in objs]
args += [str(build / (name + ".dir") / "Debug" / (name + ".lib"))
         for name in ("game-shared", "engine-shared", "json", "md5", "zlib", "rust-bridge-shared")]
args += [str(build / "debug/ddnet_engine_shared.lib"), "/link", "/INCREMENTAL:NO",
         "ws2_32.lib", "bcrypt.lib", "advapi32.lib", "userenv.lib", "ntdll.lib",
         "shlwapi.lib", "shell32.lib", "ole32.lib", "user32.lib", "winmm.lib"]
subprocess.run(args, env=env, check=True)
subprocess.run([str(build / 'avoid_test.exe')], env=env, check=True)
