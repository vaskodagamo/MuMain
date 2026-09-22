"""Required raw motion, every posed corner and authored raw-normal gates."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
import audit_dome
import raw_motion
import posed_source
import authored_normals
folder=ROOT/'House04'
audit_dome.matrix.compare_asset(folder)
audit_dome.normal_motion(folder)
raw_motion.audit()
posed_source.audit()
authored_normals.audit()
