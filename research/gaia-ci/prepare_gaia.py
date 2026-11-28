#!/usr/bin/env python3
"""Patch only the pinned Gaia detector, in a disposable checkout."""
import hashlib,pathlib,subprocess,sys,shutil
root=pathlib.Path(sys.argv[1]).resolve(); here=pathlib.Path(__file__).resolve().parent
pin='c229692045465a76233f9fba9197fb22bbfb3694'
assert subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()==pin
p=root/'Simulator/Modules/CollisionDetector/ClothContactDetector.cpp'
raw=p.read_bytes();blob=hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()
assert blob=='2ca1cd78d08a89aa6efcaa97614a071a4aacb2d9',('unexpected source',blob)
s=raw.decode();anchor='bool GAIA::ClothContactDetector::contactQueryVF('
assert s.count(anchor)==1
s=s.replace(anchor,'#include "kyoto_gaia_bridge.hpp"\n\n'+anchor)
for kind,name in enumerate(('targetMeshFacesScene','targetMeshVerticesScene','targetMeshEdgesScene')):
    call=f'rtcPointQuery({name}, &query, &context, nullptr, (void*)pResult);'
    assert s.count(call)==1,(name,s.count(call))
    s=s.replace(call,f'if (!kyoto_gaia::dispatch({kind}, this, query, context, pResult))\n\t\t{call}')
shutil.copy2(here/'gaia_bridge.hpp',p.parent/'kyoto_gaia_bridge.hpp')
p.write_text(s+'\n'+(here/'gaia_probe.inc').read_text())
print('Patched only original detector translation unit; all original callbacks retained.')
