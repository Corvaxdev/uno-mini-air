"""Original 5x7 bitmap alphabet, built locally as a tiny embedded font."""
from pathlib import Path
import json

PATTERNS = {
'0':'01110/10001/10011/10101/11001/10001/01110', '1':'00100/01100/00100/00100/00100/00100/01110',
'2':'01110/10001/00001/00010/00100/01000/11111', '3':'11110/00001/00001/01110/00001/00001/11110',
'4':'00010/00110/01010/10010/11111/00010/00010', '5':'11111/10000/10000/11110/00001/00001/11110',
'6':'01110/10000/10000/11110/10001/10001/01110', '7':'11111/00001/00010/00100/01000/01000/01000',
'8':'01110/10001/10001/01110/10001/10001/01110', '9':'01110/10001/10001/01111/00001/00001/01110',
'A':'01110/10001/10001/11111/10001/10001/10001', 'B':'11110/10001/10001/11110/10001/10001/11110',
'C':'01111/10000/10000/10000/10000/10000/01111', 'D':'11110/10001/10001/10001/10001/10001/11110',
'E':'11111/10000/10000/11110/10000/10000/11111', 'F':'11111/10000/10000/11110/10000/10000/10000',
'G':'01111/10000/10000/10111/10001/10001/01111', 'H':'10001/10001/10001/11111/10001/10001/10001',
'I':'11111/00100/00100/00100/00100/00100/11111', 'J':'00111/00010/00010/00010/10010/10010/01100',
'K':'10001/10010/10100/11000/10100/10010/10001', 'L':'10000/10000/10000/10000/10000/10000/11111',
'M':'10001/11011/10101/10101/10001/10001/10001', 'N':'10001/11001/11001/10101/10011/10011/10001',
'O':'01110/10001/10001/10001/10001/10001/10001/01110', 'P':'11110/10001/10001/11110/10000/10000/10000',
'Q':'01110/10001/10001/10001/10101/10010/01101', 'R':'11110/10001/10001/11110/10100/10010/10001',
'S':'01111/10000/10000/01110/00001/00001/11110', 'T':'11111/00100/00100/00100/00100/00100/00100',
'U':'10001/10001/10001/10001/10001/10001/01110', 'V':'10001/10001/10001/10001/10001/01010/00100',
'W':'10001/10001/10001/10101/10101/11011/10001', 'X':'10001/10001/01010/00100/01010/10001/10001',
'Y':'10001/10001/01010/00100/00100/00100/00100', 'Z':'11111/00001/00010/00100/01000/10000/11111',
' ':'00000/00000/00000/00000/00000/00000/00000', '-':'00000/00000/00000/11111/00000/00000/00000',
'.':'00000/00000/00000/00000/00000/00110/00110', ':':'00000/00110/00110/00000/00110/00110/00000',
'/':'00001/00001/00010/00100/01000/10000/10000', '%':'11001/11010/00010/00100/01000/01011/10011',
'+':'00000/00100/00100/11111/00100/00100/00000', '>':'10000/01000/00100/00010/00100/01000/10000',
'<':'00001/00010/00100/01000/00100/00010/00001', '=':'00000/00000/11111/00000/11111/00000/00000',
'!':'00100/00100/00100/00100/00100/00000/00100', '?':'01110/10001/00001/00010/00100/00000/00100',
'(':'00010/00100/01000/01000/01000/00100/00010', ')':'01000/00100/00010/00010/00010/00100/01000',
'°':'01100/10010/10010/01100/00000/00000/00000', '_':'00000/00000/00000/00000/00000/00000/11111'
}
PATTERNS['O']='01110/10001/10001/10001/10001/10001/01110'
ROOT=Path(__file__).resolve().parent
def build(font=True, compact=False):
    patterns = {c:PATTERNS[c] for c in '0123456789.-:'}
    if compact:
        # Seven printable bytes per glyph: one 5-bit row per character.
        patterns = {c:''.join(chr(int(row, 2)+32) for row in v.split('/')) for c,v in patterns.items()}
    (ROOT/'digits.json').write_text(json.dumps(patterns,separators=(',',':')))
    if not font:
        return
    from fontTools.fontBuilder import FontBuilder
    from fontTools.pens.ttGlyphPen import TTGlyphPen
    names=['.notdef']+[f'u{ord(c):04x}' for c in PATTERNS]
    fb=FontBuilder(1000,isTTF=True)
    fb.setupGlyphOrder(names)
    cmap={ord(c):f'u{ord(c):04x}' for c in PATTERNS}
    cmap.update({ord(c.lower()):f'u{ord(c):04x}' for c in PATTERNS if c.isalpha()})
    fb.setupCharacterMap(cmap)
    glyphs={}; metrics={}
    for name,pattern in [('.notdef',PATTERNS['?'])]+[(f'u{ord(c):04x}',v) for c,v in PATTERNS.items()]:
        pen=TTGlyphPen(None)
        for y,row in enumerate(pattern.split('/')):
            for x,bit in enumerate(row):
                if bit=='1':
                    left=x*100; bottom=(6-y)*100
                    pen.moveTo((left,bottom));pen.lineTo((left,bottom+100));pen.lineTo((left+100,bottom+100));pen.lineTo((left+100,bottom));pen.closePath()
        glyphs[name]=pen.glyph();metrics[name]=(600,0)
    fb.setupGlyf(glyphs);fb.setupHorizontalMetrics(metrics);fb.setupHorizontalHeader(ascent=800,descent=-200)
    fb.setupNameTable({'familyName':'Air Pixel','styleName':'Regular','uniqueFontIdentifier':'UnoMiniAirPixel01','fullName':'Air Pixel Regular','psName':'AirPixel-Regular','version':'Version 1.0'})
    fb.setupOS2(sTypoAscender=800,sTypoDescender=-200,usWinAscent=800,usWinDescent=200)
    fb.setupPost();fb.setupMaxp();fb.font['head'].created=fb.font['head'].modified=3800000000
    fb.font.recalcTimestamp=False;fb.font.flavor='woff2';fb.save(ROOT/'pixel.woff2')
    (ROOT/'digits.json').write_text(json.dumps({c:PATTERNS[c] for c in '0123456789.-:'},separators=(',',':')))
if __name__=='__main__':build()
