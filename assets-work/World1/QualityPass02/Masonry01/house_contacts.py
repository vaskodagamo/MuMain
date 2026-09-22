"""Review the actual closest aligned and stacked HouseEtc01 placements."""
from pathlib import Path
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import assemblies

joins = assemblies.joins
joins.append_asset = assemblies.append_asset
joins.REVIEW = assemblies.ROOT / 'review-assemblies'
original_camera = joins.set_camera
joins.set_camera = lambda bounds: original_camera(bounds, scale_factor=2.2)
joins.review_group('house-stack', 'HouseEtc01', 200, 30)
joins.review_group('house-adjacent', 'HouseEtc01', 250, 32)
