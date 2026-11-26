"""Compare structural derivative sparsity without claiming it predicts runtime."""
import json
from pass54_adaptive_discovery import geometry
from pass56_parametric import ParametricModel
from pass57_identity import IdentityModel

def run():
    rows=[]
    for kind,keys in [('vertex',((0,6),)),('replica8',tuple((i*8,i*8+6) for i in range(8)))]:
        x,t,v=geometry(kind,True)
        for cls in (ParametricModel,IdentityModel):
            m=cls(x,t,v,12);m.cache=m.models.view(None);solver,*_=m.build(keys)
            jac=solver.get_function('nlp_jac_g').sparsity_out(1)
            hess=solver.get_function('nlp_hess_l').sparsity_out(0)
            rows.append(dict(kind=kind,arm=cls.__name__,jac_shape=jac.shape,jac_nnz=jac.nnz(),hess_shape=hess.shape,hess_nnz=hess.nnz()))
    return rows
if __name__=='__main__':print(json.dumps(run(),indent=2))
