"""Rebuild in an isolated directory. Never uploads or edits the source checkout."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def run(args, log=None):
    p = subprocess.run([str(a) for a in args], text=True, encoding='utf-8', errors='replace', capture_output=True)
    if log:
        log.write_text(p.stdout + p.stderr, encoding='utf-8')
    if p.returncode:
        raise RuntimeError(p.stdout + p.stderr)
    return p.stdout

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cli', default=os.environ.get('ARDUINO_CLI', 'arduino-cli'))
    ap.add_argument('--config', type=Path, help='Arduino CLI config with installed AVR 1.8.8')
    ap.add_argument('--output', type=Path, required=True, help='New empty build directory')
    args = ap.parse_args()
    out = args.output.resolve()
    if out.exists() and any(out.iterdir()):
        raise SystemExit('Output must be new or empty; existing evidence is never overwritten')
    out.mkdir(parents=True, exist_ok=True)
    temp = out / 'tmp'; temp.mkdir()
    os.environ['TEMP'] = os.environ['TMP'] = str(temp)
    cli = [args.cli] + (['--config-file', str(args.config.resolve())] if args.config else [])
    versions = json.loads(run(cli + ['version', '--format', 'json']))
    if versions.get('VersionString') != '1.5.1':
        raise SystemExit('Pinned Arduino CLI 1.5.1 is required')
    cores = json.loads(run(cli + ['core', 'list', '--format', 'json']))
    installed = cores.get('platforms', []) if isinstance(cores, dict) else cores
    if not any(p.get('id') == 'arduino:avr' and p.get('installed_version') == '1.8.8' for p in installed):
        raise SystemExit('Install the pinned core: arduino-cli core install arduino:avr@1.8.8')
    stage = out / 'source'; stage.mkdir()
    for name in ['compact_html.py', 'pack_site.py', 'make_font.py', 'site.template.html', 'terra-ui.html', 'lab.template.html', 'navigation.html', 'theme.html', 'site-meta.json']:
        shutil.copy2(ROOT / name, stage / name)
    (stage/'tools').mkdir()
    shutil.copy2(ROOT/'tools/minify-js.cjs',stage/'tools/minify-js.cjs')
    os.environ['NODE_PATH']=os.environ.get('NODE_PATH',str(ROOT/'node_modules'))
    sketch = stage / 'firmware' / 'UnoMiniEthernet'
    shutil.copytree(ROOT / 'firmware' / 'UnoMiniEthernet', sketch)
    build = out / 'build'
    command = cli + ['compile', '--fqbn', 'arduino:avr:unomini', '--build-path', str(build),
        '--library', str(ROOT/'libraries/MiniW5500'), '--library', str(ROOT/'libraries/MiniBMP180'),
        '--library', str(ROOT/'libraries/Wire'),
        '--build-property', 'compiler.c.extra_flags=-fstack-usage -mcall-prologues',
        '--build-property', 'compiler.cpp.extra_flags=-fstack-usage -mcall-prologues',
        '--build-property', 'compiler.c.elf.extra_flags=-Wl,--relax', str(sketch)]
    properties = run(cli + ['compile', '--fqbn', 'arduino:avr:unomini', '--show-properties', str(sketch)])
    gcc = next(line.split('=',1)[1] for line in properties.splitlines() if line.startswith('runtime.tools.avr-gcc.path='))
    size = Path(gcc) / 'bin' / ('avr-size.exe' if os.name == 'nt' else 'avr-size')
    for attempt in range(8):
        run([sys.executable, stage/'pack_site.py'], out/f'asset-{attempt}.log')
        shutil.copy2(stage/'site_gz.h', sketch/'site_gz.h')
        print(f'Compile {attempt + 1}', flush=True)
        run(command, out/f'compile-{attempt}.log')
        sizes = run([size, '-A', build/'UnoMiniEthernet.ino.elf'])
        sections = {line.split()[0]:int(line.split()[1]) for line in sizes.splitlines() if line.startswith('.')}
        flash = sections['.text'] + sections['.data']; ram = sections['.data'] + sections['.bss']
        meta = json.loads((stage/'site-meta.json').read_text())
        desired = {'flash_kib':f'{flash/1024:.1f}', 'static_ram':ram, 'free_kib':f'{(32256-flash)/1024:.1f}', 'flash_pct':f'{flash/32256*100:.1f}'}
        if all(meta[k] == v for k,v in desired.items()):
            break
        meta.update(desired)
        (stage/'site-meta.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='ascii')
    else:
        raise RuntimeError('Resource labels did not converge')
    if flash > 32256 or ram >= 1600:
        raise RuntimeError(f'Memory budget exceeded: Flash={flash}, static SRAM={ram}')
    (out/'elf-size.txt').write_text(sizes,encoding='ascii')
    result = {'version':meta['version'], 'edition':'public-source',
        'cli':versions['VersionString'], 'core':'arduino:avr@1.8.8',
        'flash_bytes':flash,'static_ram_bytes':ram,'uploaded':False,
        'gzip_bytes':(stage/'site.html.gz').stat().st_size,
        'sha256':{name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in
            [('site.html.gz',stage/'site.html.gz'),('UnoMiniEthernet.ino.hex',build/'UnoMiniEthernet.ino.hex')]}}
    (out/'rebuild-result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))

if __name__ == '__main__':
    main()
