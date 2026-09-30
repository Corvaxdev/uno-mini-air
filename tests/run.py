"""Local host checks. No network, hardware access, or writes to source files."""
import argparse
import os
from pathlib import Path
import subprocess
import json

ROOT = Path(__file__).resolve().parents[1]

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', required=True, type=Path)
    args = ap.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env['TMPDIR'] = str(out)
    cxx = env.get('CXX', 'g++')
    flags = ['-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
             '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    # Header function comes from the current sketch, not a copied implementation.
    sketch = (ROOT/'firmware/UnoMiniEthernet/UnoMiniEthernet.ino').read_text()
    header = sketch[sketch.index('uint16_t header('):sketch.index('__attribute__((noinline)) uint16_t jsonResponse')]
    template = (ROOT/'tests/header-template.cpp').read_text()
    a = template.index('uint16_t header(')
    b = template.index('int main()', a)
    generated = template[:a] + header + '\n' + template[b:]
    generated = generated.replace('../firmware/', (ROOT/'firmware').as_posix()+'/')
    (out/'header.cpp').write_text(generated)
    peer = sketch[sketch.index('struct HttpPeer {'):sketch.index('// Optiboot')]
    http = sketch[sketch.index('void dropPeer('):sketch.index('void setup()')]
    (out/'http.inc').write_text(peer+http)
    gzip_size = json.loads((ROOT/'evidence/public-build.json').read_text())['gzip_bytes']
    (out/'asset-size.h').write_text(f'const uint8_t SITE_GZ[{gzip_size}]={{}};\n')
    fixture=(ROOT/'tests/http-state-template.cpp').read_text().replace('../firmware/', (ROOT/'firmware').as_posix()+'/')
    (out/'http-state.cpp').write_text(fixture)
    results = {}
    for name, source, extra in [
        ('terra', ROOT/'tests/terra.cpp', ['test']),
        ('air', ROOT/'tests/air.cpp', []),
        ('light', ROOT/'tests/light.cpp', []),
        ('shelters', ROOT/'tests/shelters.cpp', []),
        ('placement', ROOT/'tests/placement.cpp', []),
        ('http', ROOT/'tests/http.cpp', []),
        ('http-state', out/'http-state.cpp', []),
        ('timer', ROOT/'tests/timer.cpp', []),
        ('sensors', ROOT/'tests/sensors.cpp', []),
        ('recovery', ROOT/'tests/recovery.cpp', []),
        ('math', ROOT/'tests/math.cpp', []),
        ('rx-race', ROOT/'tests/rx-race.cpp', []),
        ('journal', ROOT/'tests/journal.cpp', []),
        ('journal-faults', ROOT/'tests/journal-faults.cpp', []),
        ('header', out/'header.cpp', [])]:
        exe = out/name
        includes = ['-I', str(ROOT/'tests/rx-stubs')] if name == 'rx-race' else []
        if name in ('sensors','recovery','math'): includes = ['-I',str(ROOT/'tests/sensor-stubs'),'-I',str(ROOT/'libraries/MiniBMP180/src'),'-I',str(ROOT/'firmware/UnoMiniEthernet')]
        subprocess.run([cxx, *flags, *includes, str(source), '-o', str(exe)], env=env, check=True)
        p = subprocess.run([str(exe), *extra], env=env, text=True, capture_output=True)
        (out/(name+'.log')).write_text(p.stdout+p.stderr)
        if p.returncode:
            raise RuntimeError(p.stdout+p.stderr)
        lines = (p.stdout+p.stderr).splitlines()
        results[name] = [line for line in lines if 'PASS' in line]
        print(name+': '+'; '.join(results[name]), flush=True)
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n')

if __name__ == '__main__':
    main()
