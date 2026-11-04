#!/usr/bin/env python3
"""Losslessly group captured PTY cells by equal attributes; no product logic."""
import html,json,sys
from pathlib import Path

def colour(c, default):
    names={'default':default,'black':'000000','red':'cd0000','green':'00cd00','brown':'cdcd00','blue':'0000ee','magenta':'cd00cd','cyan':'00cdcd','white':'e5e5e5','brightblack':'7f7f7f','brightwhite':'ffffff'}
    v=names.get(c,c)
    return '#'+v if len(v)==6 else '#'+default

for p in Path(sys.argv[1]).rglob('*.json'):
    d=json.loads(p.read_text())
    if 'cells' not in d:continue
    rows=[]
    for row in d.pop('cells'):
        runs=[]
        for x,c in enumerate(row):
            attrs={k:v for k,v in c.items() if k!='text'}
            if not runs or runs[-1]['attributes']!=attrs:
                runs.append({'start':x,'text_cells':[],'attributes':attrs})
            runs[-1]['text_cells'].append(c['text'])
        assert sum(len(r['text_cells']) for r in runs)==d['columns']
        rows.append(runs)
    d['cell_encoding']='lossless attribute runs: each text_cells item is one terminal cell, including empty wide-glyph continuation cells'
    d['cell_runs']=rows
    p.write_text(json.dumps(d,ensure_ascii=False,separators=(',',':'))+'\n')
    rendered=[]
    for runs in rows:
        line=[]
        for r in runs:
            c=r['attributes'];fg=colour(c['fg'],'dddddd');bg=colour(c['bg'],'000000')
            if c['reverse']:fg,bg=bg,fg
            line.append('<span style="color:'+fg+';background:'+bg+'">'+html.escape(''.join(r['text_cells']))+'</span>')
        rendered.append(''.join(line))
    p.with_suffix('.html').write_text('<!doctype html><meta charset="utf-8"><title>PTY cell projection</title><style>body{background:#222;color:white;font:13px monospace}pre{font:14px/1.25 Menlo,monospace;white-space:pre}</style><p>REAL PTY CELL PROJECTION — '+str(d['columns'])+'×'+str(d['rows'])+'; not a native Terminal screenshot. Browser font metrics are illustrative. DEC ACS line-drawing may be projected as letters by pyte.</p><pre>'+"\n".join(rendered)+'</pre>')
