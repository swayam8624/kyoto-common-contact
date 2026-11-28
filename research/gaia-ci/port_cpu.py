#!/usr/bin/env python3
"""Pinned upstream CPU cloth-only portability audit. No contact equations replaced."""
from pathlib import Path
import sys,re,subprocess
root=Path(sys.argv[1]).resolve()
m=root/'Simulator/Modules'
cu=root/'Simulator/3rdParty/CuMatrix'
mf=root/'Simulator/3rdParty/MeshFrame2/MeshFrame/MeshFrame'
def edit(p,f):
    s=p.read_text(); t=f(s)
    assert t!=s,p
    p.write_text(t)
def rep(s,a,b):
    assert s.count(a)==1,(a,s.count(a))
    return s.replace(a,b,1)
def guard(s,a,b):
    i=s.index(a);j=s.index(b,i+len(a))
    return s[:i]+'#ifndef KYOTO_GAIA_CLOTH_ONLY\n'+s[i:j]+'#endif\n'+s[j:]
for rel in ['CuMatrix/Geometry/Geometry.h','CuMatrix/MatrixOps/CuMatrix.h','CuMatrix/MatrixOps/CuMatrixDefs.h']:
    def host(s):
        s=rep(s,'#include "cuda_runtime.h"','#ifndef KYOTO_CUMATRIX_HOST_ONLY\n#include "cuda_runtime.h"\n#endif')
        if rel.endswith('CuMatrixDefs.h'):
            s=rep(s,'#define GPU_CPU_INLINE_FUNC  __forceinline__ __device__ __host__\n#define GPU_CPU_FUNC_NO_INLINE  __device__ __host__','#ifdef KYOTO_CUMATRIX_HOST_ONLY\n#define GPU_CPU_INLINE_FUNC inline\n#define GPU_CPU_FUNC_NO_INLINE\n#else\n#define GPU_CPU_INLINE_FUNC  __forceinline__ __device__ __host__\n#define GPU_CPU_FUNC_NO_INLINE  __device__ __host__\n#endif')
            a=s.index('#define CUDA_CHECK_RET(');b=s.index('#define CMP_EPSILON ',a)
            s=s[:a]+'#ifndef KYOTO_CUMATRIX_HOST_ONLY\n'+s[a:b]+'#endif\n'+s[b:]
            s=rep(s,'#include <iostream>','#include <iostream>\n#include <cmath>\n#include <cstdint>')
        else:
            s=rep(s,'#include "device_launch_parameters.h"','#ifndef KYOTO_CUMATRIX_HOST_ONLY\n#include "device_launch_parameters.h"\n#endif')
            if rel.endswith('/CuMatrix.h'):
                a=s.index('template <class Func, typename DType>\n__global__ void parallel_for_3x3_matOps')
                assert s[a:].count('__global__')==2
                s=s[:a]+'#ifndef KYOTO_CUMATRIX_HOST_ONLY\n'+s[a:]+'\n#endif\n'
        return s
    edit(cu/rel,host)
edit(m/'Parallelization/CPUParallelization.h',lambda s:rep(s,'Func & func','const Func & func'))
def cloth(s):
    s=rep(s,'#include "../TetMesh/TetMeshFEM.h"','#ifndef KYOTO_GAIA_CLOTH_ONLY\n#include "../TetMesh/TetMeshFEM.h"\n#endif')
    s=guard(s,'\t\tTriMeshForCollision(TetMeshFEM* PTetMesh)','\t\tTriMeshForCollision(TriMeshFEM* pTriMesh)')
    return guard(s,'\tinline void computeContactNormalTetMesh(','\tinline void computeVFContactNormalTriMesh(')
edit(m/'CollisionDetector/CollisionGeometry.h',cloth)
def base(s):
    s=rep(s,'template<typename DType, typename VertexType, typename EdgeType, typename FaceType, typename HalfEdgeType>\n\tclass CMeshBase','template<typename ScalarType, typename VertexType, typename EdgeType, typename FaceType, typename HalfEdgeType>\n\tclass CMeshBase')
    s=rep(s,'\t\ttypedef DType DType;','')
    return rep(s,'\t\ttypedef TVec3<DType> Vec3;','\t\ttypedef ScalarType DType;\n\t\ttypedef TVec3<DType> Vec3;')
edit(mf/'TriMesh/BaseMesh.h',base)
def parser(s):
    s,n=re.subn(r'SAFE_STRCPY\((mToken|currentString),\s*""\)',lambda m:m[1]+"[0] = '\\0'",s)
    assert n==7
    return s
edit(mf/'Parser/strutil.h',parser)
def ply(s):
    s=rep(s,'#define _strdup _strdup','#define _strdup strdup')
    # Dynamic filename buffer has strlen(filename)+5 bytes by construction.
    s=rep(s,'\n\tSAFE_STRCPY(name, filename);','\n\tstd::memcpy(name, filename, std::strlen(filename)+1);')
    s=rep(s,'SAFE_STRCAT(name, ".ply");','std::memcpy(name+std::strlen(name), ".ply", 5);')
    # All remaining active calls target the original fixed-size name arrays.
    s=re.sub(r'^(\s*)SAFE_STRCPY\(([^,]+), ([^;]+)\);',r'\1std::snprintf(\2, sizeof(\2), "%s", \3);',s,flags=re.M)
    return rep(s,'#define USE_PLY_WRAPPER 1','#include <cstring>\n#include <cstdio>\n#define USE_PLY_WRAPPER 1')
edit(mf/'FileIO/PlyFile.h',ply)
edit(mf/'TriMesh/VertexStatic.h',lambda s:rep(s,'\t\t\tTVec3<DType> pos;','\t\t\tTVerticesMat<DType>* mPVertPos = nullptr;\n\t\t\tTVec3<DType> pos;'))
edit(mf/'TriMesh/Iterators2.h',lambda s:rep(s,'MEIterator(MeshPtr pM, MPIterator<EdgeType>& iter)','MEIterator(MeshPtr pM, MPIterator<EdgeType> iter)'))
def io(s):
    s=rep(s,'#include <algorithm>','#include <algorithm>\n#ifndef _WIN32\n#include <filesystem>\n#endif')
    a='bool IO::folderExists(const char* folderName)\n{';b='bool IO::copyFile('
    i=s.index(a);j=s.index(b,i)
    s=s[:i]+'#ifdef _WIN32\n'+s[i:j]+'#else\n'+a+'\n std::error_code ec; return std::filesystem::is_directory(folderName,ec);\n}\n#endif\n'+s[j:]
    a='bool IO::createFolder(std::string folderName)\n{';i=s.index(a)
    return s[:i]+'#ifdef _WIN32\n'+s[i:]+'\n#else\n'+a+'\n std::error_code ec; if(std::filesystem::is_directory(folderName,ec)) return false; ec.clear(); std::filesystem::create_directories(folderName,ec); return bool(ec) || !std::filesystem::is_directory(folderName,ec);\n}\n#endif\n'
edit(mf/'Utility/IO.cpp',io)
edit(m/'TriMesh/TriMesh.cpp',lambda s:rep(s,'<MeshFrame/Utility/STR.h>','<MeshFrame/Utility/Str.h>'))

# The upstream Windows source uses case-insensitive paths. Resolve known Eigen
# casing and local quoted headers on the case-sensitive Linux filesystem.
for directory in (m,mf):
    for p in directory.rglob('*'):
        if p.suffix not in ('.h','.hpp','.cpp','.inl','.cuh','.cu'):continue
        s=p.read_text(errors='surrogateescape')
        t=re.sub(r'(#\s*include\s*[<"])Eigen/core([>"])',r'\1Eigen/Core\2',s)
        def casefix(match):
            name=match[2]
            cur=p.parent
            if not name.startswith(('.', '..')) and '/' not in name and not (cur/name).exists():
                pass
            for part in name.split('/'):
                if part=='..':cur=cur.parent;continue
                if part=='.':continue
                target=cur/part
                if target.exists():cur=target;continue
                if not cur.is_dir():return match[0]
                candidates=[x for x in cur.iterdir() if x.name.casefold()==part.casefold()]
                if len(candidates)!=1:return match[0]
                cur=candidates[0]
            if cur.is_file():
                import os
                n=os.path.relpath(cur,p.parent).replace(os.sep,'/')
                if not (p.parent/name).exists():return match[1]+n+'"'
            return match[0]
        t=re.sub(r'(^\s*#\s*include\s*")([^"\n]+)"',casefix,t,flags=re.M)
        if t!=s:p.write_text(t,errors='surrogateescape')
