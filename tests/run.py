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
             '-fno-omit-frame-pointer']
    # Header function comes from the current sketch, not a copied implementation.
    sketch = (ROOT/'firmware/UnoMiniEthernet/UnoMiniEthernet.ino').read_text()
    header = sketch[sketch.index('uint16_t header('):sketch.index('__attribute__((noinline)) uint16_t jsonResponse')]
    template = (ROOT/'tests/header-template.cpp').read_text()
    a = template.index('uint16_t header(')
    b = template.index('int main()', a)
    generated = template[:a] + header + '\n' + template[b:]
    generated = generated.replace('../firmware/', (ROOT/'firmware').as_posix()+'/')
    (out/'header.cpp').write_text(generated)
    results = {}
    for name, source, extra in [
        ('terra', ROOT/'tests/terra.cpp', ['test']),
        ('air', ROOT/'tests/air.cpp', []),
        ('light', ROOT/'tests/light.cpp', []),
        ('shelters', ROOT/'tests/shelters.cpp', []),
        ('placement', ROOT/'tests/placement.cpp', []),
        ('http', ROOT/'tests/http.cpp', []),
        ('header', out/'header.cpp', [])]:
        exe = out/name
        subprocess.run([cxx, *flags, str(source), '-o', str(exe)], env=env, check=True)
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
