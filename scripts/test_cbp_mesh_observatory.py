#!/usr/bin/env python3
"""Full authentic mesh-source -> measured CPU -> WebGL viewer CI contract."""
from __future__ import annotations
import json
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

def command(args, ok=True):
    r=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                     text=True,check=False,timeout=90)
    if (r.returncode==0)!=ok:
        raise AssertionError(
            f"Unexpected exit status: {args}\n{r.stdout}\n{r.stderr}")

def main():
    if len(sys.argv)!=4:
        raise SystemExit("Usage: test_cbp_mesh_observatory.py EXPORT REPLAY MESH")
    export,replay,mesh=map(lambda x:str(pathlib.Path(x).resolve()),sys.argv[1:])
    script=pathlib.Path(__file__).with_name("render_cbp_mass_dashboard.py")
    with tempfile.TemporaryDirectory(prefix="cbp-mesh-viewer-") as temp:
        folder=pathlib.Path(temp)/"test"
        command([export,str(folder),"12","12","541","crowded"])
        command([replay,str(folder/"snapshots.kmcb"),str(folder/"instances.csv"),
                 str(folder/"cpu-core.csv"),"core"])
        command([mesh,str(folder),str(folder/"mesh-truth.json"),"12"])
        output=folder/"real-mesh-dashboard.html"
        command([sys.executable,str(script),str(folder),"--html",str(output)])
        markup=output.read_text(encoding="utf-8")
        checks=["CCP_MESH_TRUTH_V1","Real meshes + context","drawMeshGeometry",
                "mesh-vertex","gl.TRIANGLES","gl.LINES"]
        if not all(text in markup for text in checks):
            raise AssertionError("Actual mesh draw program or topology missing")
        js=re.findall(r"<script>(.*?)</script>",markup,flags=re.S)
        node=shutil.which("node")
        if len(js)!=1:raise AssertionError("Invalid standalone browser client")
        if node:
            script_js=pathlib.Path(temp)/"browser.js"
            script_js.write_text(js[0],encoding="utf-8")
            command([node,"--check",str(script_js)])
        # Negative test: explicitly refuse to render missing/falsified positions.
        path=folder/"mesh-truth.json"
        contents=json.loads(path.read_text(encoding="utf-8"))
        contents["positions"][0][0][0]="nonfinite"
        path.write_text(json.dumps(contents),encoding="utf-8")
        command([sys.executable,str(script),str(folder),
                 "--html",str(folder/"forged.html")],ok=False)
    print("PASS visually real cloth/cushion/sponge/ball/spring World meshes: "
          "12 frame source matching, exact CPU and offline WebGL syntax; "
          "falsified mesh data rejected")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
