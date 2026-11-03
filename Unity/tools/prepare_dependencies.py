#!/usr/bin/env python3
"""Merge FITD's Unity package dependencies without replacing project choices."""
import json
import sys
from pathlib import Path
project=Path(sys.argv[1]).resolve()
package=Path(__file__).resolve().parents[1]/'Package'/'package.json'
manifest=project/'Packages'/'manifest.json'
manifest.parent.mkdir(parents=True,exist_ok=True)
data=json.loads(manifest.read_text()) if manifest.exists() else {'dependencies':{}}
for name,version in json.loads(package.read_text())['dependencies'].items():
    data.setdefault('dependencies',{}).setdefault(name,version)
manifest.write_text(json.dumps(data,indent=2)+'\n')
