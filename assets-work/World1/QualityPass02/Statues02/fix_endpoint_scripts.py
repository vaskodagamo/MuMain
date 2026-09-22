"""Apply exact perimeter endpoint precision to the two sculptural pier build entrypoints."""
from pathlib import Path
ROOT=Path(__file__).resolve().parent
for name in ('build_piers.py','build_pillar2.py'):
 path=ROOT/name;text=path.read_text()
 old=' poly=[(p,u) for p,u,b in rows]\n for lo,hi,lower,upper in zip(levels,levels[1:],insets,insets[1:]):'
 new=' poly=[(p,u) for p,u,b in rows]\n levels=list(levels);levels[0]=min(p.z for p,u in poly);levels[-1]=max(p.z for p,u in poly)\n for lo,hi,lower,upper in zip(levels,levels[1:],insets,insets[1:]):'
 assert old in text or new in text
 path.write_text(text.replace(old,new))
