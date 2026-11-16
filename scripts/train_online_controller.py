#!/usr/bin/env python3
"""Train a tiny policy-cost MLP in PyTorch without leaking simulation frames.

INPUT: CSVs from ccp_material_replay. Four teacher strategies have *separate,
long-lived caches* on the same exact AABB trajectories. Predictions are
counterfactual single-policy costs, NOT a proven switched-controller return.
EVALUATION: split WHOLE independent seed/material/regime trajectories, tune
only on validation, evaluate held-out test once, then export small versioned
C++-loadable MLP weights. Strict failures for incomplete/uncertified data.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import platform
import random
import subprocess
import sys
import time
from dataclasses import dataclass, asdict
from statistics import mean, median

try:
    import numpy as np
    import torch
    from torch import nn
except ImportError as exc:
    raise SystemExit("Install dependencies: python3 -m pip install torch numpy") from exc

POLICIES=("sap_rebuild","global_guarded_exact",
          "selective_guard_exact","region_hierarchical")
NFEATURES=12
FEATURES=tuple("f"+str(i) for i in range(NFEATURES))
EXPORTED_VERSION="KYOTO_MLP_V1"

@dataclass(frozen=True)
class Config:
    depth:int
    width:int
    lr:float
    weight_decay:float
    dropout:float
    batch:int

def sha256(path:Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda:handle.read(1024*1024),b""):
            h.update(chunk)
    return h.hexdigest()

def enumerate_csv(paths):
    files=[]
    for candidate in paths:
        path=Path(candidate).expanduser()
        if path.is_dir():
            files.extend(sorted(path.rglob("*-cpu.csv")))
        elif path.is_file():
            files.append(path)
        else:
            files.extend(sorted(Path().glob(str(path))))
    seen=set()
    for path in sorted(files):
        resolved=path.resolve()
        if resolved in seen:continue
        seen.add(resolved)
        yield resolved

def load(paths):
    data=[]
    meta=[]
    for path in enumerate_csv(paths):
        with path.open(newline="",encoding="utf-8") as stream:
            reader=csv.DictReader(stream)
            required=set(FEATURES)|{"backend","n","material","seed","frame",
                  "ms","pairs","pair_hash","misses"}
            if not required.issubset(reader.fieldnames or ()):
                raise ValueError(f"CSV missing training columns: {path}; rebuild current ccp_material_replay")
            rows=list(reader)
        by_frame={}
        for row in rows:
            if None in row or any(v is None for v in row.values()):
                raise ValueError(f"Malformed teacher CSV header/field count: {path}")
            name=row["backend"]
            if name not in POLICIES:continue
            frame=int(row["frame"])
            key=(name,frame)
            if key in by_frame:raise ValueError(f"Duplicate backend/frame: {path}, {key}")
            by_frame[key]=row
        frames=sorted({frame for _,frame in by_frame})
        if not frames or frames!=list(range(len(frames))):
            raise ValueError(f"Noncontiguous frame history in {path}")
        signature={(r["seed"],r["material"],r["n"]) for r in by_frame.values()}
        if len(signature)!=1:raise ValueError(f"Mixed metadata in {path}")
        seed,material,count=next(iter(signature))
        # A complete file is a single independent trajectory. A seed-based
        # split separates ALL scenes that share that deterministic seed.
        for frame in frames:
            trial=[by_frame.get((p,frame)) for p in POLICIES]
            if any(row is None for row in trial):
                raise ValueError(f"Missing baseline in {path}, frame {frame}")
            if len({(r["pairs"],r["pair_hash"]) for r in trial})!=1:
                raise ValueError(f"Different collision pair sets: {path}, frame {frame}")
            if any(int(r["misses"]) for r in trial):
                raise ValueError(f"Incorrect collision pair oracle: {path}")
            features=np.array([float(trial[0][key]) for key in FEATURES],dtype=np.float64)
            for r in trial[1:]:
                if not np.allclose(features,np.array([float(r[key]) for key in FEATURES]),
                                   rtol=1e-7,atol=1e-8):
                    raise ValueError(f"Teacher features disagree: {path}, frame {frame}")
            costs=np.array([float(r["ms"]) for r in trial],dtype=np.float64)
            if not np.isfinite(features).all() or not np.isfinite(costs).all() or (costs<=0).any():
                raise ValueError(f"Invalid numeric training data: {path}, frame {frame}")
            data.append({"features":features,"costs":costs,"seed":str(seed),
                         "material":str(material),"n":str(count),
                         "regime":path.stem.split("-")[2] if len(path.stem.split("-"))>=4 else "unknown",
                         "trajectory":str(path),"frame":frame})
        meta.append({"path":str(path),"sha256":sha256(path),"frames":len(frames),
                     "seed":seed,"material":material,"n":count})
    if not data:raise ValueError("No valid CPU replay CSVs supplied")
    # Fail closed on duplicate trajectory dumps: repeated runs can share one
    # seed, but identical content must not silently increase sample weight.
    hashes=[item["sha256"] for item in meta]
    if len(set(hashes))!=len(hashes):
        raise ValueError("Duplicate identical training files supplied")
    return data,meta

def split_samples(data,mode,seed):
    key=lambda r:r[mode]
    groups=sorted({key(r) for r in data})
    if len(groups)<5:
        raise ValueError(f"Only {len(groups)} independent {mode} groups: need >=5 "
                         "for train/validation/test leakage protection")
    random.Random(seed).shuffle(groups)
    nt=max(1,int(round(len(groups)*.2)))
    nv=max(1,int(round(len(groups)*.2)))
    train=set(groups[nt+nv:]);val=set(groups[nt:nt+nv]);test=set(groups[:nt])
    assert train and val and test and not (train&val or val&test or test&train)
    part=lambda allowed:[r for r in data if key(r) in allowed]
    return part(train),part(val),part(test),{
        "train":sorted(train),"validation":sorted(val),"test":sorted(test),
        "grouping":mode}

def tensors(rows,mean_,std_,device):
    x=np.stack([r["features"] for r in rows]).astype(np.float32)
    y=np.stack([np.log(r["costs"]) for r in rows]).astype(np.float32)
    x=np.clip((x-mean_)/std_,-8,8).astype(np.float32)
    # NumPy float32 x minus float64 mean/std silently promotes to float64.
    # Keep torch inputs float32, matching nn.Linear default weights exactly.
    return (torch.as_tensor(x,device=device),
            torch.as_tensor(y,device=device))

def model_for(config:Config):
    if config.depth==0:
        return nn.Sequential(nn.Linear(NFEATURES,len(POLICIES)))
    layers=[]
    width=config.width
    for index in range(config.depth):
        layers.append(nn.Linear(NFEATURES if index==0 else width,width))
        layers.append(nn.ReLU())
        if config.dropout:layers.append(nn.Dropout(config.dropout))
    layers.append(nn.Linear(width,len(POLICIES)))
    return nn.Sequential(*layers)

def train_one(config,train,val,mean_,std_,device,seed,max_epochs,patience,min_delta,
              collect=False,progress=None,progress_every=25):
    random.seed(seed);np.random.seed(seed);torch.manual_seed(seed)
    model=model_for(config).to(device)
    x,y=tensors(train,mean_,std_,device)
    vx,vy=tensors(val,mean_,std_,device)
    opt=torch.optim.AdamW(model.parameters(),lr=config.lr,
                          weight_decay=config.weight_decay)
    loss=nn.SmoothL1Loss(beta=.25)
    best=float("inf");best_state=None;best_epoch=0;stale=0
    history=[]
    for epoch in range(1,max_epochs+1):
        model.train()
        indices=torch.randperm(x.shape[0],device=device)
        total=0.
        for begin in range(0,len(indices),config.batch):
            batch=indices[begin:begin+config.batch]
            opt.zero_grad(set_to_none=True)
            objective=loss(model(x[batch]),y[batch])
            if not torch.isfinite(objective):raise FloatingPointError("NaN in training")
            objective.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(),max_norm=5.)
            opt.step()
            total+=float(objective.detach())*len(batch)
        model.eval()
        with torch.inference_mode():
            validation=float(loss(model(vx),vy))
        history.append({"epoch":epoch,"train_huber":total/len(indices),
                        "validation_huber":validation})
        if progress and (epoch==1 or epoch%progress_every==0 or epoch==max_epochs):
            progress(epoch,max_epochs,total/len(indices),validation,best,stale)
        if validation<best-min_delta:
            best=validation;best_epoch=epoch;stale=0
            best_state={key:val.detach().cpu().clone()
                        for key,val in model.state_dict().items()}
        else:
            stale+=1
        if stale>=patience:break
    if best_state is None:raise RuntimeError("No valid checkpoint")
    model.load_state_dict(best_state)
    model.eval()
    return model,best,best_epoch,history if collect else []

def grid():
    configurations=[]
    for depth,width in [(0,0),(1,8),(1,16),(1,32),
                        (2,8),(2,16),(2,32)]:
        for lr,decay,dropout,batch in itertools.product(
                (3e-4,1e-3),(0.,1e-4,1e-3),
                ((0.,) if depth==0 else (0.,.1)),(64,256)):
            configurations.append(Config(depth,width,lr,decay,dropout,batch))
    return configurations

def quality(model,rows,mean_,std_,device):
    model.eval()
    x,_=tensors(rows,mean_,std_,device)
    with torch.inference_mode():
        predicted=np.exp(np.clip(model(x).cpu().numpy(),-16,10))
    truth=np.stack([row["costs"] for row in rows])
    chosen=np.argmin(predicted,axis=1)
    actual=truth[np.arange(len(chosen)),chosen]
    ideal=np.min(truth,axis=1)
    baselines={p:float(np.mean(truth[:,i])) for i,p in enumerate(POLICIES)}
    return {"samples":len(rows),
            "top1_cost_choice_accuracy":float(np.mean(chosen==truth.argmin(axis=1))),
            "actual_mean_chosen_ms":float(np.mean(actual)),
            "oracle_unattainable_mean_ms":float(np.mean(ideal)),
            "counterfactual_regret_mean_ms":float(np.mean(actual-ideal)),
            "counterfactual_regret_p95_ms":float(np.percentile(actual-ideal,95)),
            "static_policy_mean_ms":baselines,
            "predicted_cost_mean_ms":float(np.mean(np.min(predicted,axis=1)))}

def export_cxx(model,config,mean_,std_,path):
    linear=[layer for layer in model.modules() if isinstance(layer,nn.Linear)]
    if len(linear)!=config.depth+1:raise RuntimeError("incompatible model architecture")
    with path.open("w",encoding="utf-8") as dest:
        dest.write(f"{EXPORTED_VERSION}\n{NFEATURES} {max(1,config.width)} {config.depth} {len(POLICIES)}\n")
        for values in (mean_,std_):
            dest.write(" ".join(f"{float(v):.17g}" for v in values)+"\n")
        for layer in linear:
            for tensor in (layer.weight,layer.bias):
                values=tensor.detach().cpu().numpy().reshape(-1)
                dest.write(" ".join(f"{float(v):.17g}" for v in values)+"\n")

def git_revision():
    try:
        return subprocess.check_output(["git","rev-parse","HEAD"],text=True,
                                       stderr=subprocess.DEVNULL).strip()
    except Exception:return "unavailable"

def devlog_append(path,run):
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open("a",encoding="utf-8") as handle:
        handle.write("\n### PyTorch learned-selector experiment "+
                     str(run["run_id"])+"\n\n")
        handle.write("- Status: **"+run["status"]+"**. Dataset and trials remain in "+
                     str(run["output"])+"; no performance result is assumed from training alone.\n")
        handle.write("- Source commit: `"+run["git_revision"]+"`; split: `"+
                     run["split"]+"`; independent groups: "+
                     json.dumps(run["groups"],sort_keys=True)+".\n")
        handle.write("- Data: "+str(run["samples"])+
                     " validated paired teacher frame samples; trials: "+
                     str(run["trials_executed"])+".\n")
        handle.write("- Best validation model: `"+str(run["selected"])+"`.\n")
        handle.write("- Held-out *counterfactual* quality (not real switched runtime): `"+
                     json.dumps(run["test"],sort_keys=True)+"`.\n")
        handle.write("- Decision: evaluate artifact on unseen CPU replay trajectories with "
                     "`ccp_material_replay ... MODEL.txt` and compare measured `ml_cost_aware` "
                     "time against Global Guard, original online, Bullet and region baselines. "
                     "Do not deploy without an exact-pair oracle and positive aggregate gain.\n")

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs",nargs="+",help="CSV paths or folders with *-cpu.csv")
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--split",choices=("seed","material","regime"),default="seed")
    parser.add_argument("--search",choices=("grid","random","single"),default="grid")
    parser.add_argument("--exhaustive",action="store_true",
                        help="Evaluate the ENTIRE deterministic hyperparameter grid")
    parser.add_argument("--max-trials",type=int,default=32)
    parser.add_argument("--seed",type=int,default=20261116)
    parser.add_argument("--search-epochs",type=int,default=200)
    parser.add_argument("--final-epochs",type=int,default=700)
    parser.add_argument("--patience",type=int,default=30)
    parser.add_argument("--device",choices=("cpu","mps","cuda","auto"),default="cpu")
    parser.add_argument("--verbose",action="store_true")
    parser.add_argument("--no-devlog",action="store_true")
    parser.add_argument("--resume",action="store_true",
                        help="Continue a stopped search from existing validated trials.jsonl")
    parser.add_argument("--progress-every",type=int,default=25,
                        help="Print ongoing training epoch diagnostics (default 25)")
    args=parser.parse_args()
    if (args.max_trials<1 or args.search_epochs<1 or args.final_epochs<1 or
        args.patience<1 or args.progress_every<1):
        parser.error("invalid search budget")
    if args.resume:
        if not (args.output.is_dir() and (args.output/"trials.jsonl").is_file()):
            parser.error("--resume requires original model directory with trials.jsonl")
        if (args.output/"manifest.json").exists():
            parser.error("model manifest already exists: training completed; run C++ holdout evaluation")
    elif args.output.exists() and any(args.output.iterdir()):
        parser.error("output directory must be new/empty; --resume explicitly for interrupted runs")
    args.output.mkdir(parents=True,exist_ok=True)
    run_id=args.output.name
    data,metadata=load(args.inputs)
    train,val,test,groups=split_samples(data,args.split,args.seed)
    training=np.stack([row["features"] for row in train])
    feature_mean=training.mean(axis=0)
    feature_std=np.maximum(training.std(axis=0),1e-4)
    # PyTorch Apple MPS is optional; CPU is often faster for 629-parameter nets.
    if args.device=="auto":
        device="cuda" if torch.cuda.is_available() else (
            "mps" if torch.backends.mps.is_available() and len(train)>10000 else "cpu")
    else:device=args.device
    if device=="mps" and not torch.backends.mps.is_available():
        raise RuntimeError("MPS not available on this host")
    if device=="cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA not available on this host")
    configs=grid()
    random.Random(args.seed).shuffle(configs)
    if args.search=="single":configs=[Config(2,16,1e-3,1e-4,0.,128)]
    if not args.exhaustive:configs=configs[:args.max_trials]
    trials_path=args.output/"trials.jsonl"
    # Strict search contract is persisted BEFORE the first neural trial.
    # Older runs (like the user's first 34/156 exhaustive grid) only have
    # trials.jsonl; they are recoverable after each existing record's index,
    # configuration and validation score have been checked.
    data_digest=hashlib.sha256(
        json.dumps([(row["path"],row["sha256"]) for row in metadata],
                   separators=(",",":")).encode("utf-8")).hexdigest()
    contract={"schema":1,"dataset_sha256":data_digest,
              "seed":args.seed,"split":args.split,"groups":groups,
              "search":args.search,"device":device,
              "search_epochs":args.search_epochs,"final_epochs":args.final_epochs,
              "patience":args.patience,"configurations":[asdict(c) for c in configs]}
    contract_file=args.output/"search_contract.json"
    if args.resume:
        if contract_file.is_file():
            previous=json.loads(contract_file.read_text(encoding="utf-8"))
            if previous!=contract:
                raise RuntimeError(
                  "Refusing to resume: original datasets, split, device, grid or "
                  "training settings differ from search_contract.json")
        else:
            print("[RESUME WARNING] Legacy run has no search_contract.json. "
                  "Validating all trial indices/configurations, then anchoring "
                  "the currently present dataset hashes and settings.",flush=True)
    else:
        if contract_file.exists():
            raise RuntimeError("refusing to overwrite experiment contract")
    if not contract_file.exists():
        with contract_file.open("x",encoding="utf-8") as stream:
            json.dump(contract,stream,indent=2)
            stream.write("\n")
    completed=[]
    if args.resume:
        with trials_path.open(encoding="utf-8") as stream:
            for line_no,line in enumerate(stream,1):
                if not line.strip():continue
                try:record=json.loads(line)
                except json.JSONDecodeError as exc:
                    raise RuntimeError(
                      f"Truncated trials.jsonl at line {line_no}; preserve the "
                      "original file and inspect before resuming") from exc
                index=len(completed)
                if index>=len(configs) or record.get("trial")!=index or (
                    record.get("configuration")!=asdict(configs[index])):
                    raise RuntimeError(
                      f"Search grid mismatch at trial line {line_no}; "
                      "refusing to duplicate or overwrite prior trials")
                value=float(record["val_huber"])
                epoch=int(record["best_epoch"])
                if not math.isfinite(value) or not 1<=epoch<=args.search_epochs:
                    raise RuntimeError(
                      f"Invalid recorded validation score/epoch at line {line_no}")
                completed.append(record)
    best=float("inf");winner=None;winner_epoch=0
    for index,record in enumerate(completed):
        if record["val_huber"]<best:
            best=record["val_huber"]
            winner=configs[index]
            winner_epoch=record["best_epoch"]
    if args.resume:
        print(f"[RESUME] Reusing {len(completed)}/{len(configs)} completed "
              f"hyperparameter trials. Best validation loss {best:.6f}. "
              "Original data and previous trial lines are unchanged.",flush=True)
    for index in range(len(completed),len(configs)):
        config=configs[index]
        began=time.perf_counter()
        def progress(epoch,maximum,train_loss,val_loss,best_loss,stale):
            if args.verbose:
                print(f"[TRAIN-EPOCH] trial={index+1}/{len(configs)} "
                      f"epoch={epoch}/{maximum} train_huber={train_loss:.6f} "
                      f"val_huber={val_loss:.6f} best_so_far={best_loss:.6f} "
                      f"stale_epochs={stale} elapsed_s={time.perf_counter()-began:.1f}",
                      flush=True)
        # Validation grouping is unchanged for every trial. Holdout test is
        # never inspected until the winning architecture is frozen.
        candidate,score,epoch,_=train_one(config,train,val,feature_mean,feature_std,
                device,args.seed+index,args.search_epochs,args.patience,1e-4,
                progress=progress,progress_every=args.progress_every)
        record={"trial":index,"configuration":asdict(config),"val_huber":score,
                "best_epoch":epoch,"wall_seconds":time.perf_counter()-began}
        with trials_path.open("a",encoding="utf-8") as handle:
            handle.write(json.dumps(record)+"\n")
            handle.flush()
            os.fsync(handle.fileno())
        if score<best:
            best=score;winner=config;winner_epoch=epoch
        print(f"[TRAIN] trial={index+1}/{len(configs)} val_huber={score:.6f} "
              f"best_epoch={epoch} best_so_far={best:.6f} "
              f"elapsed_s={record['wall_seconds']:.1f}",flush=True)
    assert winner is not None
    # Select epoch by validation, then fit once on train+validation for a
    # fixed, preselected duration. No test examples influence training.
    combined=train+val
    combined_np=np.stack([r["features"] for r in combined])
    combined_mean=combined_np.mean(axis=0)
    combined_std=np.maximum(combined_np.std(axis=0),1e-4)
    fixed_epoch=min(args.final_epochs,max(1,winner_epoch))
    # Force entire epoch count without an internal validation-selected
    # checkpoint: final refit has no remaining selection partition.
    random.seed(args.seed+999);np.random.seed(args.seed+999);torch.manual_seed(args.seed+999)
    final=model_for(winner).to(device)
    x,y=tensors(combined,combined_mean,combined_std,device)
    optimizer=torch.optim.AdamW(final.parameters(),lr=winner.lr,
                                weight_decay=winner.weight_decay)
    objective=nn.SmoothL1Loss(beta=.25)
    history=[]
    for epoch in range(1,fixed_epoch+1):
        final.train()
        ix=torch.randperm(len(x),device=device)
        running=0.
        for start in range(0,len(ix),winner.batch):
            batch=ix[start:start+winner.batch]
            optimizer.zero_grad(set_to_none=True)
            loss=objective(final(x[batch]),y[batch])
            if not torch.isfinite(loss):raise FloatingPointError("NaN in final fit")
            loss.backward()
            nn.utils.clip_grad_norm_(final.parameters(),5.)
            optimizer.step()
            running+=float(loss.detach())*len(batch)
        history.append({"epoch":epoch,"train_huber":running/len(ix)})
    final.eval()
    # Evaluate test exactly once after all hyperparameters and epoch choices.
    test_metrics=quality(final,test,combined_mean,combined_std,device)
    validation_proxy=quality(final,val,combined_mean,combined_std,device)
    model_file=args.output/"controller.mlpc"
    export_cxx(final,winner,combined_mean,combined_std,model_file)
    torch.save(final.cpu().state_dict(),args.output/"controller.state_dict.pt")
    (args.output/"final_training_history.json").write_text(
        json.dumps(history,indent=2)+"\n",encoding="utf-8")
    manifest={
        "run_id":run_id,"status":"trained_heldout_not_deployed",
        "output":str(args.output),"git_revision":git_revision(),
        "torch_version":torch.__version__,"numpy_version":np.__version__,
        "python_version":sys.version.split()[0],"platform":platform.platform(),
        "device":device,"seed":args.seed,"split":args.split,"groups":groups,
        "samples":{"train":len(train),"validation":len(val),"test":len(test)},
        "teacher_policy_order":POLICIES,"feature_order":FEATURES,
        "teacher_cache_semantics":"each teacher maintained its OWN uninterrupted cache",
        "deployment_cache_semantics":"resumed destination cache INVALIDATED after switching",
        "data":metadata,"trials_executed":len(configs),
        "selected":asdict(winner),"selected_validation_huber":best,
        "selected_validation_epoch":winner_epoch,
        "final_refit_epochs":fixed_epoch,
        "test":test_metrics,"validation_post_refit":validation_proxy,
        "model_sha256":sha256(model_file),
        "warning":"Proxy labels are independent-policy frame times, NOT online switching reward. "
                  "Retest deployed controller on unseen trajectories; a trained model is not a win."
    }
    (args.output/"manifest.json").write_text(
        json.dumps(manifest,indent=2,default=str)+"\n",encoding="utf-8")
    print("[TRAIN] model:",model_file)
    print("[HELDOUT] independent-group proxy:",json.dumps(test_metrics,sort_keys=True))
    if not args.no_devlog:
        devlog_append(Path("DEVLOG.md"),manifest)
    print("[NEXT] run held-out C++ material replay with the exported model to obtain "
          "REAL switching/rebuild/inference costs. Training fit alone is not a result.")

if __name__=="__main__":
    main()
