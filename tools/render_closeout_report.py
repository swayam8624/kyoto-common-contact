#!/usr/bin/env python3
"""Render the validated closeout manifest in the scientific environment."""
import argparse
import json
from pathlib import Path
from research_closeout_report import render

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);args=p.parse_args()
    m=json.loads((args.output/'campaign_manifest.json').read_text());n=render(args.output,m)
    print(f'Rendered {n} measured graphs (SVG + PNG) and offline dashboard')
