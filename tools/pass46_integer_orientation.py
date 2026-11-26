"""Exact dyadic orientation. Common positive power-of-two scaling preserves
all determinant signs. Child Bernstein coefficients are uniformly scaled
by eight, preserving exact rational subdivision decisions."""
import numpy as np

def det(a,b,c):
    return a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])

def positive(c,d):
    a,b,e,f=c
    if min(c)>0:return True
    if min(a,f)<=0 or d==0:return False
    m=a+3*b+3*e+f
    return positive((8*a,4*(a+b),2*(a+2*b+e),m),d-1) and positive((m,2*(b+2*e+f),4*(e+f),8*f),d-1)

def batch_orientation(a,b,t,depth=10):
    a,b=np.asarray(a,dtype=float),np.asarray(b,dtype=float);t=np.asarray(t)
    if a.shape!=b.shape or a.ndim!=2 or a.shape[1]!=3 or not np.isfinite(a).all() or not np.isfinite(b).all():raise ValueError('invalid frames')
    if t.ndim!=2 or t.shape[1]!=4 or t.dtype.kind not in 'iu' or (len(t) and (t.min()<0 or t.max()>=len(a))):raise ValueError('invalid topology')
    if not isinstance(depth,int) or not 0<=depth<=20:raise ValueError('invalid depth')
    pairs=[float(x).as_integer_ratio() for x in np.concatenate((a,b)).ravel()]
    k=max((d.bit_length()-1 for _,d in pairs),default=0)
    vals=[n<<(k-(d.bit_length()-1)) for n,d in pairs]
    pts=[tuple(vals[i:i+3]) for i in range(0,len(vals),3)];A,B=pts[:len(a)],pts[len(a):]
    def cols(x,t):
        o=x[t[0]]
        return [tuple(a-b for a,b in zip(x[i],o)) for i in t[1:]]
    out=[]
    for row in t.tolist():
        u,v,w=cols(A,row);p,q,r=cols(B,row)
        out.append(positive((3*det(u,v,w),det(p,v,w)+det(u,q,w)+det(u,v,r),det(u,q,r)+det(p,v,r)+det(p,q,w),3*det(p,q,r)),depth))
    return tuple(out)
