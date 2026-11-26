"""Global LRU bound on compiled NLPs, with step-local plane memoization.
The physical equations and exact gate are unchanged. Entry count is not RSS.
"""
from collections import OrderedDict
from pass54_adaptive_discovery import DurationModel

class BoundedModels:
    def __init__(self,capacity):
        if type(capacity) is not int or capacity<1:raise ValueError('invalid capacity')
        self.capacity=capacity;self.items=OrderedDict();self.hits=self.misses=self.evictions=self.peak=0
    def view(self,duration):return DurationView(self,duration)
    def stats(self):return dict(capacity=self.capacity,resident=len(self.items),peak=self.peak,hits=self.hits,misses=self.misses,evictions=self.evictions)

class DurationView:
    def __init__(self,owner,duration):self.owner=owner;self.duration=duration
    def __contains__(self,keys):
        o=self.owner;present=(self.duration,keys) in o.items
        if present:o.hits+=1
        else:o.misses+=1
        return present
    def __getitem__(self,keys):
        k=(self.duration,keys);self.owner.items.move_to_end(k);return self.owner.items[k]
    def __setitem__(self,keys,value):
        o=self.owner;k=(self.duration,keys)
        if k not in o.items and len(o.items)==o.capacity:o.items.popitem(last=False);o.evictions+=1
        o.items[k]=value;o.items.move_to_end(k);o.peak=max(o.peak,len(o.items))

class BoundedDurationModel(DurationModel):
    def __init__(self,x,t,v,capacity=12):
        super().__init__(x,t,v);self.models=BoundedModels(capacity);self._planes=None;self.plane_hits=0;self.plane_misses=0
    def plane(self,x,key):
        if self._planes is None:return super().plane(x,key)
        if key in self._planes:self.plane_hits+=1;return self._planes[key]
        self.plane_misses+=1;r=super().plane(x,key);self._planes[key]=r;return r
    def solve(self,x,v,duration,budget):
        self.dt=float(duration);self.cache=self.models.view(duration);self._planes={}
        try:return self.step(x,v,True,budget=budget)
        finally:self._planes=None
