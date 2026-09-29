"""Reproducible ASCII gzip assets, streamed from MCU Flash."""
from pathlib import Path
import hashlib, json, re, subprocess, os
import zopfli.gzip
from importlib.metadata import version
from make_font import build
from compact_html import compact_html

ROOT = Path(__file__).resolve().parent
meta = json.loads((ROOT/'site-meta.json').read_text(encoding='ascii'))
chat_enabled = False
navigation = (ROOT/'navigation.html').read_text(encoding='ascii')
navigation_style = re.search(r'<style>(.*?)</style>', navigation, re.S)[1]
navigation_body = navigation.split('</style>', 1)[1].strip()

def add_navigation(template, active=None):
    body = navigation_body
    if active:
        body = body.replace('href="' + active + '"', 'href="' + active + '" aria-current="page"', 1)
    return template.replace('/*NAVIGATION_STYLE*/', navigation_style).replace('<!--NAVIGATION-->', body)

if version('zopfli') != '0.4.1':
    raise RuntimeError('Zopfli 0.4.1 required')

def compact_scripts(html):
    def script(match):
        cmd = [os.environ.get('NODE', 'node'), str(ROOT/'tools/minify-js.cjs')]
        p = subprocess.run(cmd, input=match[1], text=True, capture_output=True, check=True)
        return '<script>' + p.stdout.strip() + '</script>'
    return re.sub(r'<script>(.*?)</script>', script, html, flags=re.S)

def parts(name, css_id, page_class):
    template = add_navigation((ROOT/name).read_text(encoding='ascii'), '/' if page_class == 'live-page' else None)
    style = re.search(r'<style>(.*?)</style>', template, re.S)
    body, script = template[style.end():].rsplit('<script>', 1)
    script = script.split('</script>')[0]
    body = body.replace('class="page"', 'class="page ' + page_class + '"', 1)
    control = ''
    assert body.count('<!--THEME-->') == 1
    body = body.replace('<!--THEME-->', control, 1)
    css = '<style id="' + css_id + '"' + (' media="not all"' if page_class == 'details-page' else '') + '>' + style[1] + '</style>'
    return css, body, script

build(font=False, compact=True)
live_css, live_body, live_js = parts('site.template.html', 'live-style', 'live-page')
live_css = live_css.replace('/*STORIES_STYLE*/', '')
live_body = live_body.replace('<!--STORIES_BODY-->', '')
live_js = 'const chatEnabled=false;' + live_js.replace('//STORIES_SCRIPT', '')
terra = (ROOT/'terra-ui.html').read_text(encoding='ascii')
terra_css = re.search(r'<style>(.*?)</style>', terra, re.S)
terra_body, terra_js = terra[terra_css.end():].rsplit('<script>',1)
live_css = live_css.replace('/*TERRA_STYLE*/',terra_css[1])
live_body = live_body.replace('<!--TERRA_BODY-->',terra_body)
live_js = live_js.replace('//TERRA_SCRIPT',terra_js.split('</script>')[0])
detail_css, detail_body, _ = parts('lab.template.html', 'details-style', 'details-page')
theme = (ROOT/'theme.html').read_text(encoding='ascii')
html = '<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AIR / 328 - Arduino UNO Mini Web Server</title><meta name="description" content="Live sensors and a shared ecosystem on Arduino UNO Mini Limited Edition: 32 KiB Flash, 2 KiB RAM."><meta property="og:title" content="AIR / 328 - Arduino UNO Mini Web Server"><meta property="og:description" content="Live sensors and a shared ecosystem on Arduino UNO Mini Limited Edition: 32 KiB Flash, 2 KiB RAM."><meta property="og:type" content="website">'
html = html.replace('<html lang="en">', '<html lang="en"><head>', 1)
html += theme + live_css + detail_css
html += '<script>const detailsPage=location.pathname==="/lab"||location.pathname==="/devlog",devlog=location.pathname==="/devlog";document.documentElement.classList.add(detailsPage?(devlog?"devlog":"lab"):"live");if(detailsPage){document.getElementById("details-style").media="all";document.title=(devlog?"Arduino development log":"Arduino server tests and memory")+" - AIR / 328";document.querySelector("meta[name=description]").content=devlog?"Firmware milestones: sensors, EEPROM wear leveling, I2C recovery and a room-driven ecosystem on the ATmega328P.":"Measured Internet loads, Flash and RAM use, and engineering behind the Arduino UNO Mini HTTP server."}const canonical=document.createElement("link");canonical.rel="canonical";canonical.href=location.origin+(detailsPage?location.pathname:"/");document.head.appendChild(canonical)</script>'
html += '</head><body>' + live_body + detail_body
html += '<script>(()=>{if(detailsPage){document.querySelector(devlog?\'.details-page .nav a[href="/devlog"]\':\'.details-page .nav a[href="/lab"]\').setAttribute("aria-current","page");return;}' + live_js + '})()</script></body></html>'
html = html.replace('__DIGITS__', (ROOT/'digits.json').read_text(encoding='ascii'))
values = {**meta, 'ram_pct':round(meta['static_ram']/2048*100, 1), 'ram_free':2048-meta['static_ram']}
for key, value in values.items():
    html = html.replace('__' + key.upper() + '__', str(value))
if re.search(r'__[A-Z_]+__', html):
    raise ValueError('Unresolved template variable')
private_ids=sorted(set(re.findall(r'id="(terra-[a-z-]+)"',html)))
for i,name in enumerate(private_ids):
    html=html.replace(name,'z'+str(i))
raw = compact_html(compact_scripts(html)).encode('ascii')
packed = zopfli.gzip.compress(raw, numiterations=50)
for name in ['site', 'lab']:
    (ROOT/(name+'.html')).write_bytes(raw)
    (ROOT/(name+'.html.gz')).write_bytes(packed)
guide_raw=raw; guide_packed=packed
(ROOT/'guide.html').write_bytes(raw)
(ROOT/'guide.html.gz').write_bytes(packed)
lines = [','.join(str(b) for b in packed[i:i+24]) for i in range(0, len(packed), 24)]
header = '#pragma once\n#include <avr/pgmspace.h>\n#define SITE_VERSION "' + meta['firmware'] + '"\nconst uint8_t SITE_GZ[] PROGMEM = {\n' + ',\n'.join(lines) + '\n};\n#define LAB_GZ SITE_GZ\n'
(ROOT/'site_gz.h').write_text(header, encoding='ascii')
asset = {'html_bytes':len(raw), 'gzip_bytes':len(packed), 'html_sha256':hashlib.sha256(raw).hexdigest(), 'gzip_sha256':hashlib.sha256(packed).hexdigest(), 'ascii_only':True}
manifest = {**meta, **asset, 'font_bytes':0, 'shared_document':True, 'lab':asset, 'guide':{'html_bytes':len(guide_raw),'gzip_bytes':len(guide_packed),'gzip_sha256':hashlib.sha256(guide_packed).hexdigest()}}
(ROOT/'asset-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='ascii')
print(json.dumps(manifest))
