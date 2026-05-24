#!/usr/bin/env python3
"""
Stable Audio 3 API Service

Serves the `medium` (ARC-distilled) model + a default LoRA to the gary4local
VST, from the gary-backend-spark compose network. Async generation with
polling — the exact same poll_status contract the JUCE frontend already
speaks to foundation-1.

v1 scope: POST /generate (text-to-audio, NO bpm/bar alignment — that is
/generate/loop later) + GET /poll_status/<id> + /health + /ready.

GPU access is serialized by gpu-queue-service on the dedicated `sa3` lane
(concurrency=1, mirror of the foundation lane).
"""

from flask import Flask, request, jsonify
import torch
import torchaudio
from einops import rearrange
import io
import json
import base64
import uuid
import os
import time
import threading
import gc
import ctypes
import random
import re
import requests as http_requests

from stable_audio_3.pipeline import StableAudioPipeline
from stable_audio_3.inference.distribution_shift import (
    LogSNRShift,
    FluxDistributionShift,
    DistributionShift,
    IdentityDistributionShift,
)

app = Flask(__name__)

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

MODEL_NAME = os.environ.get("SA3_MODEL", "medium")          # ARC-distilled
MODEL_HALF = os.environ.get("SA3_MODEL_HALF", "1") != "0"

# Every .ckpt/.safetensors in LORA_DIR is preloaded at warmup as a fixed
# registry index (deterministic sorted order so indices are stable across
# restarts). Per request, each LoRA's strength + sigma-interval is driven
# independently — mirrors the validated gradio multi-LoRA blend. New files
# are picked up via POST /reload.
LORA_DIR = os.environ.get("SA3_LORA_DIR", "/app/loras")
# Live-editable dice pools (bind-mounted, read fresh per request — edits take
# effect with no rebuild/restart; that's the point of the endpoint).
PROMPTS_DIR = os.environ.get("SA3_PROMPTS_DIR", "/app/prompts")
LORA_EXTS = (".ckpt", ".safetensors")
# Name (file stem) used when a request doesn't specify any LoRA.
DEFAULT_LORA_NAME = os.environ.get("SA3_DEFAULT_LORA_NAME", "sa3_patch_baseline")
OUTPUT_DIR = os.environ.get("OUTPUT_DIR", "/app/outputs")
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Generation defaults for `medium` (ARC). CFG is effectively always 1.0 on the
# distilled model — APG / cfg-rescale / cfg-norm are inert at cfg=1 so they are
# not part of the request surface (sane defaults baked into the pipeline call).
DEFAULT_STEPS = int(os.environ.get("SA3_DEFAULT_STEPS", "8"))
DEFAULT_CFG = float(os.environ.get("SA3_DEFAULT_CFG", "1.0"))
DEFAULT_SAMPLER = os.environ.get("SA3_DEFAULT_SAMPLER", "pingpong")
DEFAULT_NEGATIVE = os.environ.get("SA3_DEFAULT_NEGATIVE", "low quality")
DEFAULT_DURATION = float(os.environ.get("SA3_DEFAULT_DURATION", "30"))
MAX_DURATION = float(os.environ.get("SA3_MAX_DURATION", "300"))

# Free GPU memory after each generation (in-process unload, ace-step
# /v1/unload pattern — proven on this Spark). Default OFF: we're in UX
# iteration, cold-load would slow it. Flip on to benchmark whether
# reclaiming VRAM between requests hurts UX. Lazy reload on next request.
MANAGE_MODEL_LIFECYCLE = os.environ.get(
    "MANAGE_MODEL_LIFECYCLE", "off"
).lower() in ("on", "true", "1")

# Schedule-shift *type* is exposed; per-shift sliders are not. When the caller
# asks for the same family the model already ships, we reuse the model's tuned
# instance (the params gradio shows on init); otherwise we construct that
# family with library defaults. "default" => model's own shift; "none" => no
# shift at all.
VALID_SHIFTS = ["default", "none", "logsnr", "flux", "full"]

# Legacy single-LoRA selectors (still accepted when "loras" list is absent).
VALID_LORA = ["default", "none"]

# /generate/loop: DAW passes BPM inside the prompt (e.g. "... 124 bpm ...").
# Bar→seconds is 4/4: seconds_per_bar = (60/bpm)*4. We do NOT trust the model
# to emit a sample-exact odd-second length — generate LOOP_PAD_SECONDS extra,
# then hard-trim to round(loop_duration*sr) so the loop is DAW-bar-exact.
VALID_LOOP_BARS = [4, 8, 16, 32]
DEFAULT_LOOP_BARS = int(os.environ.get("SA3_DEFAULT_LOOP_BARS", "8"))
LOOP_PAD_SECONDS = float(os.environ.get("SA3_LOOP_PAD_SECONDS", "2.0"))
_BPM_RE = re.compile(r"(\d+(?:\.\d+)?)\s*bpm", re.IGNORECASE)


def extract_bpm(prompt: str):
    """Pull BPM out of the prompt text (DAW writes it there). Float-aware.
    Returns float or None."""
    m = _BPM_RE.search(prompt or "")
    return float(m.group(1)) if m else None

# ---------------------------------------------------------------------------
# Session store  (session_id -> job state)
# ---------------------------------------------------------------------------

sessions = {}
sessions_lock = threading.Lock()

# ---------------------------------------------------------------------------
# GPU queue integration — concurrency=1 `sa3` lane in gpu-queue-service
# ---------------------------------------------------------------------------
QUEUE_URL = os.environ.get("QUEUE_URL", "http://gpu-queue:8085").rstrip("/")


def _acquire_gpu_slot(session_id: str, timeout: float = 600.0) -> bool:
    """Register on the sa3 lane and block until our slot is ready.
    Returns True if we got the slot, False on timeout/error."""
    try:
        resp = http_requests.post(
            f"{QUEUE_URL}/sa3/tasks",
            json={"session_id": session_id},
            timeout=10,
        )
        if resp.status_code != 200:
            print(f"[{session_id}] queue register failed: {resp.status_code} {resp.text}")
            return False

        data = resp.json()
        if data.get("status", "") == "processing":
            print(f"[{session_id}] GPU slot acquired immediately")
            return True

        print(f"[{session_id}] Queued at position {data.get('position', '?')}, polling...")
        deadline = time.time() + timeout
        while time.time() < deadline:
            time.sleep(0.5)
            try:
                poll = http_requests.get(
                    f"{QUEUE_URL}/sa3/tasks/{session_id}", timeout=5
                )
                if poll.status_code == 200:
                    pdata = poll.json()
                    if pdata.get("status", "") == "processing":
                        print(f"[{session_id}] GPU slot acquired after queuing")
                        return True
                    update_session(session_id, queue_position=pdata.get("position", 0))
            except Exception as e:
                print(f"[{session_id}] queue poll error: {e}")

        print(f"[{session_id}] GPU slot timeout after {timeout}s")
        return False

    except Exception as e:
        # If gpu-queue-service is unreachable, proceed so SA3 can still operate
        # standalone during development / recovery.
        print(f"[{session_id}] queue acquire error (proceeding anyway): {e}")
        return True


def _release_gpu_slot(session_id: str, status: str = "completed"):
    """Tell gpu-queue-service we're done so the next sa3 task can proceed."""
    try:
        http_requests.post(
            f"{QUEUE_URL}/sa3/task/status",
            json={"session_id": session_id, "status": status},
            timeout=5,
        )
    except Exception as e:
        print(f"[{session_id}] queue release error: {e}")


def create_session(session_id: str, meta: dict):
    with sessions_lock:
        sessions[session_id] = {
            "status": "queued",   # queued -> generating -> encoding -> completed/failed
            "generation_in_progress": True,
            "transform_in_progress": False,
            "progress": 0,
            "step": 0,
            "total_steps": meta.get("steps", DEFAULT_STEPS),
            "audio_data": None,
            "error": None,
            "meta": meta,
            "queue_position": 0,
            "created_at": time.time(),
        }


def update_session(session_id: str, **kwargs):
    with sessions_lock:
        if session_id in sessions:
            sessions[session_id].update(kwargs)


def get_session(session_id: str) -> dict | None:
    with sessions_lock:
        return sessions.get(session_id, {}).copy() if session_id in sessions else None


def cleanup_old_sessions(max_age: float = 1800.0):
    now = time.time()
    with sessions_lock:
        expired = [
            sid for sid, s in sessions.items()
            if now - s.get("created_at", 0) > max_age
        ]
        for sid in expired:
            del sessions[sid]


# ---------------------------------------------------------------------------
# Model management — pipeline stays warm; LoRA loaded once at warmup
# ---------------------------------------------------------------------------

_pipe = None
pipe_lock = threading.Lock()       # guards pipeline (re)build / unload
pipe_ready = threading.Event()     # warmup done at least once (service accepts)
gen_lock = threading.Lock()        # serializes set_lora_strength + generate
model_resident = False             # is the model currently on the GPU?
last_load_seconds = 0.0            # wall time of the most recent (cold) load
model_sample_rate = None           # cached so /health needn't touch the model
model_device = None


def cuda_mem_mb():
    """Snapshot of the CUDA memory pool — used to verify the unload actually
    reclaims VRAM on the GB10's unified memory, not just that it's fast."""
    if not torch.cuda.is_available():
        return None
    free, total = torch.cuda.mem_get_info()
    return {
        "allocated_mb": round(torch.cuda.memory_allocated() / 1048576, 1),
        "reserved_mb": round(torch.cuda.memory_reserved() / 1048576, 1),
        "free_mb": round(free / 1048576, 1),
        "total_mb": round(total / 1048576, 1),
    }

# Ordered registry: index -> {"name": <file stem>, "path": <abs path>}.
# Order is the sorted file order so a given LoRA keeps its index across
# restarts (clients can address by name regardless).
lora_registry = []
lora_name_to_index = {}


def scan_lora_dir():
    """Return [(name, path), ...] for every LoRA file in LORA_DIR, sorted by
    filename so registry indices are deterministic."""
    if not os.path.isdir(LORA_DIR):
        return []
    files = sorted(
        f for f in os.listdir(LORA_DIR) if f.endswith(LORA_EXTS)
    )
    return [(os.path.splitext(f)[0], os.path.join(LORA_DIR, f)) for f in files]


def aggressive_cleanup():
    for _ in range(3):
        gc.collect()
    try:
        ctypes.CDLL("libc.so.6").malloc_trim(0)
    except Exception:
        pass
    if torch.cuda.is_available():
        torch.cuda.empty_cache()


def load_pipeline(force_rebuild: bool = False):
    """Construct the pipeline and preload every LoRA in LORA_DIR as a fixed
    registry index. Warm. `force_rebuild` rebuilds from scratch (used by
    /reload to pick up newly-added LoRA files — load_lora structurally mutates
    the model and has no incremental add, so a fresh build is the clean path)."""
    global _pipe, lora_registry, lora_name_to_index, model_resident
    global last_load_seconds, model_sample_rate, model_device
    with pipe_lock:
        if _pipe is not None and not force_rebuild:
            return _pipe

        if force_rebuild:
            print("Rebuilding Stable Audio 3 pipeline (/reload)...")
            pipe_ready.clear()
            _pipe = None
            aggressive_cleanup()

        t0 = time.time()
        print(f"Loading Stable Audio 3 pipeline: model={MODEL_NAME} half={MODEL_HALF}")
        pipe = StableAudioPipeline.from_pretrained(MODEL_NAME, model_half=MODEL_HALF)

        registry = scan_lora_dir()
        if registry:
            paths = [p for _, p in registry]
            print(f"  Preloading {len(paths)} LoRA(s): "
                  f"{[n for n, _ in registry]}")
            pipe.load_lora(paths)
            loaded = getattr(pipe.model, "lora_names", [])
            print(f"  LoRA registry (index->name): "
                  f"{dict(enumerate(loaded))}")
        else:
            print(f"  No LoRA files in {LORA_DIR} — serving base model only")

        lora_registry = registry
        lora_name_to_index = {name: i for i, (name, _) in enumerate(registry)}

        _pipe = pipe
        model_resident = True
        last_load_seconds = round(time.time() - t0, 2)
        model_sample_rate = pipe.model_config.get("sample_rate")
        model_device = str(pipe.device)
        sr = model_sample_rate
        print(f"  Pipeline ready in {last_load_seconds}s. sample_rate={sr} "
              f"diffusion_objective={getattr(pipe.model, 'diffusion_objective', '?')} "
              f"mem={cuda_mem_mb()}")
        pipe_ready.set()
        return _pipe


def unload_pipeline():
    """Drop the pipeline and free GPU memory in-process. Mirrors ace-step's
    /v1/unload (double gc + empty_cache + synchronize), proven to reclaim VRAM
    on this Spark. Simpler here: the pipeline object owns model/dit/pretransform
    /conditioner as attributes, so dropping _pipe frees them together — no
    persistent-handler dangling-alias problem. Lazy reload on next request."""
    global _pipe, model_resident
    with pipe_lock:
        if _pipe is None:
            model_resident = False
            return {"status": "already_unloaded", "mem": cuda_mem_mb()}

        if torch.cuda.is_available():
            torch.cuda.synchronize()
        before = cuda_mem_mb()

        _pipe = None
        model_resident = False

        for _ in range(2):          # second pass breaks reference cycles
            gc.collect()
        try:
            # NB: `import torch._dynamo` would bind `torch` as a function-local
            # and UnboundLocalError the torch.cuda calls above. Use importlib.
            import importlib
            importlib.import_module("torch._dynamo").reset()
        except Exception:
            pass
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
            torch.cuda.synchronize()
        try:
            ctypes.CDLL("libc.so.6").malloc_trim(0)
        except Exception:
            pass

        after = cuda_mem_mb()
        freed = (round(before["allocated_mb"] - after["allocated_mb"], 1)
                 if before and after else None)
        print(f"[unload] freed ~{freed} MB allocated "
              f"(before={before} after={after})")
        return {"status": "unloaded", "freed_mb": freed,
                "before": before, "after": after}


# ---------------------------------------------------------------------------
# Param resolution
# ---------------------------------------------------------------------------

def resolve_dist_shift(pipe, shift: str):
    """Map the requested shift *type* to a dist_shift instance (or None).

    "default" -> None  (pipeline falls back to model.sampling_dist_shift)
    "none"    -> IdentityDistributionShift (timesteps unchanged)
    logsnr/flux/full -> reuse the model's tuned instance if it's that family,
                        else that family constructed with library defaults.
    """
    s = (shift or "default").lower()
    if s == "default":
        return None
    if s == "none":
        return IdentityDistributionShift()

    families = {
        "logsnr": LogSNRShift,
        "flux": FluxDistributionShift,
        "full": DistributionShift,
    }
    cls = families.get(s)
    if cls is None:
        raise ValueError(f"shift must be one of {VALID_SHIFTS}")

    model_default = getattr(pipe.model, "sampling_dist_shift", None)
    if isinstance(model_default, cls):
        return model_default          # keep the tuned params gradio shows on init
    return cls()                      # different family => library defaults


def resolve_loras(data: dict) -> list:
    """Normalize the request's LoRA selection into a list of per-index configs:
        [{"lora_index", "name", "strength", "interval", "layer_filter"}, ...]

    Two input forms:
      - "loras": [{"name","strength","interval_min","interval_max","layer_filter"}]
      - legacy "lora" ("default"|"none") + "lora_strength"

    Raises ValueError (→ 400) on unknown name / bad shape. Registry must be
    loaded (caller checks pipe_ready first).
    """
    entries = data.get("loras")

    if entries is None:
        # Legacy single-LoRA path.
        sel = (data.get("lora") or "default").lower()
        if sel not in VALID_LORA:
            raise ValueError(f"lora must be one of {VALID_LORA} (or use 'loras')")
        if sel == "none" or not lora_registry:
            return []
        if DEFAULT_LORA_NAME not in lora_name_to_index:
            raise ValueError(
                f"default LoRA '{DEFAULT_LORA_NAME}' not in registry "
                f"{list(lora_name_to_index)}"
            )
        return [{
            "lora_index": lora_name_to_index[DEFAULT_LORA_NAME],
            "name": DEFAULT_LORA_NAME,
            "strength": float(data.get("lora_strength", 1.0)),
            "interval": (0.0, 1.0),
            "layer_filter": "",
        }]

    if not isinstance(entries, list):
        raise ValueError("'loras' must be a list")

    resolved = []
    for e in entries:
        if not isinstance(e, dict) or "name" not in e:
            raise ValueError("each 'loras' entry needs at least a 'name'")
        name = e["name"]
        if name not in lora_name_to_index:
            raise ValueError(
                f"unknown LoRA '{name}'. available: {list(lora_name_to_index)}"
            )
        imin = float(e.get("interval_min", 0.0))
        imax = float(e.get("interval_max", 1.0))
        if not (0.0 <= imin <= imax <= 1.0):
            raise ValueError(
                f"LoRA '{name}': require 0 <= interval_min <= interval_max <= 1"
            )
        resolved.append({
            "lora_index": lora_name_to_index[name],
            "name": name,
            "strength": float(e.get("strength", 1.0)),
            "interval": (imin, imax),
            "layer_filter": str(e.get("layer_filter", "") or ""),
        })
    return resolved


def validate_request(data: dict) -> list:
    errors = []

    if not (data.get("prompt") or "").strip():
        errors.append("prompt is required")

    duration = data.get("duration", DEFAULT_DURATION)
    try:
        duration = float(duration)
        if duration <= 0 or duration > MAX_DURATION:
            errors.append(f"duration must be in (0, {MAX_DURATION}] seconds")
    except (TypeError, ValueError):
        errors.append("duration must be a number")

    steps = data.get("steps", DEFAULT_STEPS)
    try:
        steps = int(steps)
        if steps < 1 or steps > 200:
            errors.append("steps must be in [1, 200]")
    except (TypeError, ValueError):
        errors.append("steps must be an integer")

    cfg = data.get("cfg_scale", DEFAULT_CFG)
    try:
        cfg = float(cfg)
        if cfg < 0 or cfg > 25:
            errors.append("cfg_scale must be in [0, 25]")
    except (TypeError, ValueError):
        errors.append("cfg_scale must be a number")

    shift = (data.get("shift") or "default").lower()
    if shift not in VALID_SHIFTS:
        errors.append(f"shift must be one of {VALID_SHIFTS}")

    # LoRA selection (name resolution against the registry) is validated in
    # resolve_loras() after the warmup/ready gate, since it needs the registry.
    return errors


# ---------------------------------------------------------------------------
# Background generation worker
# ---------------------------------------------------------------------------

def generation_worker(session_id: str, params: dict):
    t_start = time.time()
    pipe = None          # local ref must be dropped before unload can free VRAM

    acquired = _acquire_gpu_slot(session_id)
    if not acquired:
        update_session(session_id, status="failed", generation_in_progress=False,
                        error="GPU busy — could not acquire sa3 queue slot")
        return

    try:
        prompt = params["prompt"]
        negative_prompt = params["negative_prompt"]
        duration = params["duration"]
        steps = params["steps"]
        cfg_scale = params["cfg_scale"]
        seed = params["seed"]
        shift = params["shift"]
        sampler_type = params["sampler_type"]
        loras = params["loras"]  # resolved list of per-index configs

        update_session(session_id, status="generating", progress=0)
        print(f"[{session_id}] generate: steps={steps} cfg={cfg_scale} "
              f"dur={duration}s shift={shift} sampler={sampler_type} seed={seed}")
        print(f"[{session_id}]   loras: "
              f"{[(c['name'], c['strength'], c['interval']) for c in loras]}")
        print(f"[{session_id}]   prompt: {prompt}")
        print(f"[{session_id}]   negative: {negative_prompt}")

        def on_step(callback_info):
            i = callback_info.get("i", 0) + 1
            update_session(session_id, step=i,
                           progress=min(90, int(i / max(steps, 1) * 90)))

        # gen_lock spans the whole load -> set_lora_strength -> generate
        # critical section. The warm model + LoRA strengths are global state;
        # holding the lock across the load too is what lets /unload, /load and
        # /reload safely 409 for the entire generation (not just the sampler
        # window) and prevents a manual unload from yanking the model between
        # load and generate. The sa3 queue lane serializes normal traffic;
        # this also covers the queue-unreachable fallback path.
        with gen_lock:
            was_resident = model_resident
            pipe = load_pipeline()       # cold-loads here if lifecycle unloaded it
            cold_load_seconds = 0.0 if was_resident else last_load_seconds
            if not was_resident:
                print(f"[{session_id}] cold-loaded model in "
                      f"{cold_load_seconds}s (MANAGE_MODEL_LIFECYCLE on)")
            sample_rate = int(pipe.model_config["sample_rate"])
            n_loras = len(getattr(pipe.model, "lora_names", []))
            dist_shift = resolve_dist_shift(pipe, shift)

            requested = {c["lora_index"]: c for c in loras}
            # Every registry index: requested -> its strength; others -> 0
            # (= that LoRA contributes nothing this request, no reload).
            for idx in range(n_loras):
                pipe.set_lora_strength(
                    requested[idx]["strength"] if idx in requested else 0.0,
                    lora_index=idx,
                )
            # lora_configs drives per-LoRA sigma-interval + layer_filter gating
            # inside the DiT forward (dit.py:458-470), exactly like gradio.
            lora_configs = [
                {
                    "lora_index": idx,
                    "interval": requested[idx]["interval"]
                    if idx in requested else (0.0, 1.0),
                    "layer_filter": requested[idx]["layer_filter"]
                    if idx in requested else "",
                }
                for idx in range(n_loras)
            ] if n_loras else None

            gen_kwargs = dict(
                prompt=prompt,
                negative_prompt=negative_prompt or None,
                duration=duration,
                steps=steps,
                cfg_scale=cfg_scale,
                seed=seed,
                dist_shift=dist_shift,
                sampler_type=sampler_type,
                lora_configs=lora_configs,
                callback=on_step,
            )
            # /transform: init_audio drives a style transfer; init_noise_level
            # (request "strength") = how far from the input (0.01 preserve →
            # 1.0 full transform). Absent for /generate and /generate/loop.
            init_audio = params.get("init_audio")
            if init_audio is not None:
                gen_kwargs["init_audio"] = init_audio
                gen_kwargs["init_noise_level"] = params.get(
                    "init_noise_level", 0.9
                )

            # /continue: inpaint continuation. Source occupies [0, source_dur]
            # (mask=1, kept); the model regenerates [source_dur, total]
            # (mask=0) — "fill forward from end of source". The pipeline builds
            # the mask from these seconds and downsamples it to latent space.
            cont = params.get("continue")
            if cont is not None:
                gen_kwargs["inpaint_audio"] = cont["inpaint_audio"]
                gen_kwargs["inpaint_mask_start_seconds"] = cont["mask_start_seconds"]
                gen_kwargs["inpaint_mask_end_seconds"] = cont["mask_end_seconds"]

            audio = pipe.generate(**gen_kwargs)

        update_session(session_id, status="encoding", progress=92)

        # [b, d, n] -> [d, b*n] int16 WAV (mirror gradio's save path)
        audio = rearrange(audio, "b d n -> d (b n)")
        audio = audio.to(torch.float32).clamp(-1, 1).mul(32767).to(torch.int16).cpu()

        # Sample-exact length: /generate/loop (bar-aligned) and /transform
        # (match the DAW input) both set target_samples. We always generate a
        # little long, then hard-trim here so the result is sample-exact
        # (zero-pad only if somehow short — defensive).
        loop = params.get("loop")
        transform = params.get("transform")
        tgt = params.get("target_samples")
        if tgt:
            have = audio.shape[-1]
            if have >= tgt:
                audio = audio[:, :tgt]
            else:
                print(f"[{session_id}] WARN short: have {have} < target "
                      f"{tgt} samples; zero-padding {tgt - have}")
                audio = torch.nn.functional.pad(audio, (0, tgt - have))

        final_duration = audio.shape[-1] / sample_rate

        buf = io.BytesIO()
        torchaudio.save(buf, audio, sample_rate, format="wav")
        audio_bytes = buf.getvalue()
        audio_b64 = base64.b64encode(audio_bytes).decode("utf-8")

        cont = params.get("continue")
        prefix = ("sa3cont" if cont else "sa3loop" if loop
                  else "sa3xform" if transform else "sa3")
        filename = f"{prefix}_{session_id}_{seed}.wav"
        output_path = os.path.join(OUTPUT_DIR, filename)
        with open(output_path, "wb") as f:
            f.write(audio_bytes)

        gen_time = time.time() - t_start
        print(f"[{session_id}] done in {gen_time:.2f}s -> {output_path} "
              f"({final_duration:.2f}s audio)")

        update_session(
            session_id,
            status="completed",
            generation_in_progress=False,
            transform_in_progress=False,
            progress=100,
            audio_data=audio_b64,
            meta={
                "session_id": session_id,
                "prompt": prompt,
                "negative_prompt": negative_prompt,
                "seed": seed,
                "duration": round(duration, 4),
                "final_duration": round(final_duration, 4),
                "steps": steps,
                "cfg_scale": cfg_scale,
                "shift": shift,
                "sampler_type": sampler_type,
                "loras": [
                    {"name": c["name"], "strength": c["strength"],
                     "interval": list(c["interval"]),
                     "layer_filter": c["layer_filter"]}
                    for c in loras
                ],
                "model": MODEL_NAME,
                "generation_time": round(gen_time, 2),
                "model_load_time": cold_load_seconds,   # 0 if it was warm
                "manage_lifecycle": MANAGE_MODEL_LIFECYCLE,
                "loop": ({
                    "bpm": loop["bpm"],
                    "bars": loop["bars"],
                    "seconds_per_bar": round(loop["seconds_per_bar"], 6),
                    "loop_duration": round(loop["loop_duration"], 6),
                    "gen_duration": round(loop["gen_duration"], 4),
                    "target_samples": loop["target_samples"],
                } if loop else None),
                "transform": ({
                    "strength": transform["strength"],
                    "input_duration": round(transform["input_duration"], 6),
                    "input_sr": transform["input_sr"],
                    "input_channels": transform["input_channels"],
                    "target_samples": transform["target_samples"],
                } if transform else None),
                "continue": ({
                    "mode": cont["mode"],
                    "source_duration": round(cont["source_duration"], 6),
                    "continuation_seconds": round(cont["continuation_seconds"], 6),
                    "total_duration": round(cont["total_duration"], 6),
                    "mask_start_seconds": round(cont["mask_start_seconds"], 6),
                    "mask_end_seconds": round(cont["mask_end_seconds"], 6),
                    "input_sr": cont["input_sr"],
                    "input_channels": cont["input_channels"],
                    "target_samples": cont["target_samples"],
                } if cont else None),
                "output_path": output_path,
            },
        )

    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"[{session_id}] Error: {e}")
        update_session(session_id, status="failed", generation_in_progress=False,
                        transform_in_progress=False, error=str(e))
    finally:
        # Unload BEFORE releasing the sa3 lane: the lane is concurrency=1, so
        # while we still hold it no other sa3 worker can be in load_pipeline().
        # Releasing first would let a queued worker rebuild concurrently with
        # this free (it would then delete the fresh model). Order matters.
        if MANAGE_MODEL_LIFECYCLE:
            try:
                pipe = None      # drop the worker's strong ref so GC can reclaim
                unload_pipeline()
            except Exception as e:
                print(f"[{session_id}] unload error: {e}")
        _release_gpu_slot(session_id)
        aggressive_cleanup()


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------

@app.route("/health", methods=["GET"])
def health():
    # Must NOT call load_pipeline() — health/poll traffic would force the model
    # resident and defeat MANAGE_MODEL_LIFECYCLE. Report cached state instead.
    if pipe_ready.is_set():
        return jsonify({
            "status": "healthy",
            "model": MODEL_NAME,
            "model_loaded": pipe_ready.is_set(),     # warmed at least once
            "model_resident": model_resident,        # currently on GPU?
            "manage_lifecycle": MANAGE_MODEL_LIFECYCLE,
            "last_load_seconds": last_load_seconds,
            "loras": [
                {"index": i, "name": n}
                for i, (n, _) in enumerate(lora_registry)
            ],
            "default_lora": DEFAULT_LORA_NAME,
            "device": model_device,
            "cuda_available": torch.cuda.is_available(),
            "cuda_mem": cuda_mem_mb(),
            "sample_rate": model_sample_rate,
        })
    return jsonify({"status": "starting", "model_loaded": False}), 503


@app.route("/ready", methods=["GET"])
def ready():
    if pipe_ready.is_set():
        return jsonify({"ready": True}), 200
    return jsonify({"ready": False}), 503


@app.route("/loras", methods=["GET"])
def loras():
    """List the preloaded LoRA registry (index -> name) so the UI can
    populate its blend controls."""
    return jsonify({
        "loras": [
            {"index": i, "name": n} for i, (n, _) in enumerate(lora_registry)
        ],
        "default_lora": DEFAULT_LORA_NAME,
        "lora_dir": LORA_DIR,
    })


@app.route("/reload", methods=["POST"])
def reload_loras():
    """Rescan LORA_DIR and rebuild the pipeline+registry to pick up newly
    added LoRA files without a container restart.

    Refuses while a generation holds the sa3 GPU lane (the rebuild frees and
    reconstructs the model — must not race an in-flight generate). Single-user
    beta admin op; not auth'd (matches the rest of the backend)."""
    if not gen_lock.acquire(blocking=False):
        return jsonify({
            "success": False,
            "error": "generation in progress — retry /reload when idle",
        }), 409
    try:
        before = [n for n, _ in lora_registry]
        load_pipeline(force_rebuild=True)
        after = [n for n, _ in lora_registry]
        print(f"[reload] registry {before} -> {after}")
        return jsonify({
            "success": True,
            "loras": [
                {"index": i, "name": n}
                for i, (n, _) in enumerate(lora_registry)
            ],
            "previous": before,
        })
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500
    finally:
        gen_lock.release()


@app.route("/unload", methods=["POST"])
def unload():
    """Manually free GPU memory (benchmarking / on-demand). 409 if a
    generation is in flight. Independent of MANAGE_MODEL_LIFECYCLE."""
    if not gen_lock.acquire(blocking=False):
        return jsonify({"success": False,
                        "error": "generation in progress — retry when idle"}), 409
    try:
        return jsonify({"success": True, **unload_pipeline()})
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500
    finally:
        gen_lock.release()


@app.route("/load", methods=["POST"])
def load():
    """Manually (re)load the model so the next generation is warm. Returns
    the cold-load wall time for the benchmark."""
    if not gen_lock.acquire(blocking=False):
        return jsonify({"success": False,
                        "error": "generation in progress — retry when idle"}), 409
    try:
        already = model_resident
        load_pipeline()
        return jsonify({
            "success": True,
            "status": "already_loaded" if already else "loaded",
            "load_seconds": 0.0 if already else last_load_seconds,
            "cuda_mem": cuda_mem_mb(),
        })
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500
    finally:
        gen_lock.release()


def _get_json_body():
    """Parse the request body, tolerating a missing/wrong Content-Type — a VST
    client shouldn't trip 'JSON body required' just because a header is off."""
    data = request.get_json(silent=True)
    if data is None:
        raw = request.get_data(as_text=True)
        if raw:
            try:
                data = json.loads(raw)
            except ValueError:
                data = None
    return data


def _build_params(data):
    """Shared param dict for /generate and /generate/loop. Caller has already
    validated and confirmed the model is ready. May raise ValueError (→400)."""
    return {
        "prompt": data["prompt"].strip(),
        "negative_prompt": (data.get("negative_prompt", DEFAULT_NEGATIVE) or "").strip(),
        "duration": float(data.get("duration", DEFAULT_DURATION)),
        "steps": int(data.get("steps", DEFAULT_STEPS)),
        "cfg_scale": float(data.get("cfg_scale", DEFAULT_CFG)),
        "shift": (data.get("shift") or "default").lower(),
        "sampler_type": data.get("sampler_type", DEFAULT_SAMPLER),
        # Resolve -1 to a concrete seed HERE so the response/meta always
        # reports the exact seed used — required to reproduce a take.
        "seed": (lambda s: random.randint(0, 99999) if s == -1 else s)(
            int(data.get("seed", -1))
        ),
        "loras": resolve_loras(data),
        "loop": None,
    }


@app.route("/generate", methods=["POST"])
def generate():
    """
    Accept a text-to-audio request, return a session_id immediately.
    Generation runs in background — poll /poll_status/<session_id>.

    Body (all optional except prompt):
      prompt          (str, required)
      negative_prompt (str, default "low quality")
      duration        (float seconds, default 30, max 300)
      steps           (int, default 8)
      cfg_scale       (float, default 1.0)
      shift           (default|none|logsnr|flux|full, default "default")
      sampler_type    (str, default "pingpong")
      seed            (int, default -1 = random)
      loras / lora    (see /loras; legacy lora/lora_strength supported)
    """
    cleanup_old_sessions()
    try:
        data = _get_json_body()
        if not data:
            return jsonify({"success": False, "error": "JSON body required"}), 400

        errors = validate_request(data)
        if errors:
            return jsonify({"success": False, "errors": errors}), 400

        if not pipe_ready.is_set():
            return jsonify({"success": False, "error": "loading model — warming up"}), 503

        try:
            params = _build_params(data)
        except ValueError as e:
            return jsonify({"success": False, "error": str(e)}), 400

        session_id = str(uuid.uuid4())[:12]
        create_session(session_id, {
            "prompt": params["prompt"],
            "steps": params["steps"],
            "duration": params["duration"],
        })

        threading.Thread(
            target=generation_worker, args=(session_id, params), daemon=True
        ).start()

        return jsonify({
            "success": True,
            "session_id": session_id,
            "seed": params["seed"],
            "prompt": params["prompt"],
            "duration": params["duration"],
        })

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500


@app.route("/generate/loop", methods=["POST"])
def generate_loop():
    """
    Bar-aligned loop. The DAW writes BPM into the prompt (e.g. "... 124 bpm");
    we derive the exact 4/4 loop length from BPM × bars, generate a little
    extra, then hard-trim to a sample-exact bar length. Async — poll
    /poll_status/<session_id>.

    Body: same as /generate, plus:
      bars  (int, default 8; one of [4, 8, 16, 32])
      bpm   (float, optional — overrides BPM parsed from the prompt)
    """
    cleanup_old_sessions()
    try:
        data = _get_json_body()
        if not data:
            return jsonify({"success": False, "error": "JSON body required"}), 400

        errors = validate_request(data)
        if errors:
            return jsonify({"success": False, "errors": errors}), 400

        if not pipe_ready.is_set():
            return jsonify({"success": False, "error": "loading model — warming up"}), 503

        prompt = data["prompt"].strip()
        bpm = data.get("bpm")
        bpm = float(bpm) if bpm not in (None, "") else extract_bpm(prompt)
        if not bpm or bpm <= 0:
            return jsonify({
                "success": False,
                "error": "BPM required — put it in the prompt (e.g. '124 bpm') "
                         "or pass a 'bpm' field",
            }), 400

        bars = int(data.get("bars", DEFAULT_LOOP_BARS))
        if bars not in VALID_LOOP_BARS:
            return jsonify({
                "success": False,
                "error": f"bars must be one of {VALID_LOOP_BARS}",
            }), 400

        sr = int(model_sample_rate or 44100)
        seconds_per_bar = (60.0 / bpm) * 4.0          # 4/4
        loop_duration = seconds_per_bar * bars
        gen_duration = loop_duration + LOOP_PAD_SECONDS
        target_samples = round(loop_duration * sr)

        if gen_duration > MAX_DURATION:
            return jsonify({
                "success": False,
                "error": f"{bars} bars @ {bpm} bpm = {loop_duration:.2f}s "
                         f"(+{LOOP_PAD_SECONDS}s pad) exceeds max {MAX_DURATION}s "
                         f"— pick fewer bars or a faster bpm",
            }), 400

        try:
            params = _build_params(data)
        except ValueError as e:
            return jsonify({"success": False, "error": str(e)}), 400

        # Generate the padded length; worker trims to target_samples exactly.
        params["duration"] = gen_duration
        params["target_samples"] = target_samples
        params["loop"] = {
            "bpm": bpm,
            "bars": bars,
            "seconds_per_bar": seconds_per_bar,
            "loop_duration": loop_duration,
            "gen_duration": gen_duration,
            "target_samples": target_samples,
        }

        session_id = str(uuid.uuid4())[:12]
        create_session(session_id, {
            "prompt": params["prompt"],
            "steps": params["steps"],
            "duration": gen_duration,
        })

        threading.Thread(
            target=generation_worker, args=(session_id, params), daemon=True
        ).start()

        return jsonify({
            "success": True,
            "session_id": session_id,
            "seed": params["seed"],
            "prompt": prompt,
            "bpm": bpm,
            "bars": bars,
            "seconds_per_bar": round(seconds_per_bar, 6),
            "loop_duration": round(loop_duration, 6),
            "gen_duration": round(gen_duration, 4),
            "target_samples": target_samples,
        })

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500


@app.route("/transform", methods=["POST"])
def transform():
    """
    Style-transform audio recorded in the DAW. The OUTPUT LENGTH IS THE INPUT
    LENGTH — there's no duration slider; the model regenerates the recorded
    clip toward the prompt. Sample-exact round-trip so it lines up in the DAW.
    Async — poll /poll_status/<session_id>.

    Body: same as /generate (NO duration — it's derived), plus:
      audio_data (str, required) base64-encoded WAV from the DAW
      strength   (float, default 0.9, 0.01–1.0) init_noise_level —
                 0.01 ≈ preserve input, 1.0 ≈ full transform
    """
    cleanup_old_sessions()
    try:
        data = _get_json_body()
        if not data:
            return jsonify({"success": False, "error": "JSON body required"}), 400

        errors = validate_request(data)
        if errors:
            return jsonify({"success": False, "errors": errors}), 400

        if not pipe_ready.is_set():
            return jsonify({"success": False, "error": "loading model — warming up"}), 503

        b64 = data.get("audio_data")
        if not b64:
            return jsonify({"success": False,
                            "error": "audio_data (base64 WAV) is required"}), 400
        try:
            raw = base64.b64decode(b64)
            waveform, in_sr = torchaudio.load(io.BytesIO(raw))  # [C, T] float
        except Exception as e:
            return jsonify({"success": False,
                            "error": f"could not decode audio_data: {e}"}), 400

        in_channels = int(waveform.shape[0])
        in_frames = int(waveform.shape[-1])
        input_duration = in_frames / float(in_sr)
        if input_duration <= 0:
            return jsonify({"success": False, "error": "empty input audio"}), 400
        if input_duration > MAX_DURATION:
            return jsonify({
                "success": False,
                "error": f"input audio {input_duration:.1f}s exceeds max "
                         f"{MAX_DURATION}s",
            }), 400

        strength = float(data.get("strength", 0.9))
        strength = min(1.0, max(0.01, strength))   # gradio slider range

        try:
            params = _build_params(data)
        except ValueError as e:
            return jsonify({"success": False, "error": str(e)}), 400

        # Generate a touch long (so the model isn't fighting the exact tail),
        # then the worker hard-trims to exactly the input's frame count — the
        # round-trip the DAW needs.
        params["duration"] = input_duration + 0.5
        params["target_samples"] = in_frames
        params["init_audio"] = (in_sr, waveform)
        params["init_noise_level"] = strength
        params["transform"] = {
            "strength": strength,
            "input_duration": input_duration,
            "input_sr": in_sr,
            "input_channels": in_channels,
            "target_samples": in_frames,
        }

        session_id = str(uuid.uuid4())[:12]
        create_session(session_id, {
            "prompt": params["prompt"],
            "steps": params["steps"],
            "duration": input_duration,
        })

        threading.Thread(
            target=generation_worker, args=(session_id, params), daemon=True
        ).start()

        return jsonify({
            "success": True,
            "session_id": session_id,
            "seed": params["seed"],
            "prompt": params["prompt"],
            "strength": strength,
            "input_duration": round(input_duration, 6),
            "input_sr": in_sr,
            "input_channels": in_channels,
            "target_samples": in_frames,
        })

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500


VALID_CONTINUATION_MODES = ["inpaint"]   # "latent_prefix" reserved (not yet)


@app.route("/continue", methods=["POST"])
def continue_audio():
    """
    Continue audio recorded in the DAW: keep the source, generate new audio
    that flows on from its end (inpaint "fill forward"). Output =
    source_length + continuation_seconds. Async — poll /poll_status/<id>.

    Body: same as /generate (NO duration — derived), plus:
      audio_data           (str, required) base64 WAV — the source to continue
      continuation_seconds (float, default 8.0) how much new audio after source
      continuation_mode    (str, default "inpaint"; "latent_prefix" reserved)

    The source occupies [0, source_dur] (kept); the model regenerates
    [source_dur, total] toward the prompt. The kept region is re-encoded
    through the autoencoder so it won't be bit-identical to the input — it's
    musically continuous, not a sample-join.
    """
    cleanup_old_sessions()
    try:
        data = _get_json_body()
        if not data:
            return jsonify({"success": False, "error": "JSON body required"}), 400

        errors = validate_request(data)
        if errors:
            return jsonify({"success": False, "errors": errors}), 400

        if not pipe_ready.is_set():
            return jsonify({"success": False, "error": "loading model — warming up"}), 503

        mode = (data.get("continuation_mode") or "inpaint").lower()
        if mode not in VALID_CONTINUATION_MODES:
            return jsonify({
                "success": False,
                "error": f"continuation_mode must be one of "
                         f"{VALID_CONTINUATION_MODES} (latent_prefix not yet "
                         f"implemented)",
            }), 400

        b64 = data.get("audio_data")
        if not b64:
            return jsonify({"success": False,
                            "error": "audio_data (base64 WAV) is required"}), 400
        try:
            raw = base64.b64decode(b64)
            waveform, in_sr = torchaudio.load(io.BytesIO(raw))   # [C, T]
        except Exception as e:
            return jsonify({"success": False,
                            "error": f"could not decode audio_data: {e}"}), 400

        in_channels = int(waveform.shape[0])
        in_frames = int(waveform.shape[-1])
        source_duration = in_frames / float(in_sr)
        if source_duration <= 0:
            return jsonify({"success": False, "error": "empty input audio"}), 400

        try:
            continuation_seconds = float(data.get("continuation_seconds", 8.0))
        except (TypeError, ValueError):
            return jsonify({"success": False,
                            "error": "continuation_seconds must be a number"}), 400
        if continuation_seconds <= 0:
            return jsonify({"success": False,
                            "error": "continuation_seconds must be > 0"}), 400

        total_duration = source_duration + continuation_seconds
        if total_duration > MAX_DURATION:
            return jsonify({
                "success": False,
                "error": f"source {source_duration:.1f}s + continuation "
                         f"{continuation_seconds:.1f}s = {total_duration:.1f}s "
                         f"exceeds max {MAX_DURATION}s",
            }), 400

        sr = int(model_sample_rate or 44100)
        target_samples = round(total_duration * sr)

        try:
            params = _build_params(data)
        except ValueError as e:
            return jsonify({"success": False, "error": str(e)}), 400

        # Generate a touch long; worker hard-trims to exact total length.
        params["duration"] = total_duration + 0.5
        params["target_samples"] = target_samples
        params["continue"] = {
            "mode": mode,
            "inpaint_audio": (in_sr, waveform),
            "source_duration": source_duration,
            "continuation_seconds": continuation_seconds,
            "total_duration": total_duration,
            # keep [0, source] ; regenerate [source, total]
            "mask_start_seconds": source_duration,
            "mask_end_seconds": total_duration,
            "input_sr": in_sr,
            "input_channels": in_channels,
            "target_samples": target_samples,
        }

        session_id = str(uuid.uuid4())[:12]
        create_session(session_id, {
            "prompt": params["prompt"],
            "steps": params["steps"],
            "duration": total_duration,
        })

        threading.Thread(
            target=generation_worker, args=(session_id, params), daemon=True
        ).start()

        return jsonify({
            "success": True,
            "session_id": session_id,
            "seed": params["seed"],
            "prompt": params["prompt"],
            "continuation_mode": mode,
            "source_duration": round(source_duration, 6),
            "continuation_seconds": round(continuation_seconds, 6),
            "total_duration": round(total_duration, 6),
            "input_sr": in_sr,
            "input_channels": in_channels,
            "target_samples": target_samples,
        })

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500


def _read_json(path):
    try:
        with open(path) as f:
            return json.load(f)
    except Exception:
        return None


@app.route("/prompts", methods=["GET"])
def prompts():
    """Dice-button pool. Read FRESH from the mounted PROMPTS_DIR every call so
    host edits land on the next roll — no rebuild/restart (the whole reason
    this endpoint exists; the JUCE plugin never changes).

    Query:
      lora  (optional, repeatable) registry LoRA name(s). Repeat the param
            (?lora=a&lora=b) or comma-separate (?lora=a,b) — matches the UI
            having >1 LoRA slider up. For each bucket a selected LoRA defines,
            that bucket becomes the deduped UNION across all selected LoRAs'
            pools (so one dice roll can land in either LoRA's distribution),
            replacing the generic default for that bucket; other buckets stay
            generic.

    Response (JUCE contract, multi-LoRA aware):
      { success, loras, missing_loras, available_loras,
        prompts: { version, dice: { generic, instrumental, ... }, source } }
    """
    defaults = _read_json(os.path.join(PROMPTS_DIR, "defaults.json")) or {
        "version": 1, "dice": {"generic": [], "instrumental": [], "drums": []}
    }
    dice = {k: list(v) for k, v in (defaults.get("dice") or {}).items()}

    available = []
    if os.path.isdir(PROMPTS_DIR):
        available = sorted(
            os.path.splitext(f)[0]
            for f in os.listdir(PROMPTS_DIR)
            if f.endswith(".json") and f != "defaults.json"
        )

    # Accept ?lora=a&lora=b and ?lora=a,b ; dedupe, keep order.
    loras, seen = [], set()
    for raw in request.args.getlist("lora"):
        for name in (p.strip() for p in raw.split(",")):
            if name and name not in seen:
                seen.add(name)
                loras.append(name)

    source = {"generic": "defaults.json"}
    bucket_seen = {}          # bucket -> set() for cross-LoRA dedupe
    bucket_replaced = set()   # buckets a LoRA has taken over from defaults
    missing = []
    for name in loras:
        lp = _read_json(os.path.join(PROMPTS_DIR, f"{name}.json"))
        if not (lp and isinstance(lp.get("dice"), dict)):
            missing.append(name)
            continue
        for bucket, items in lp["dice"].items():
            if bucket not in bucket_replaced:   # first LoRA clears the default
                dice[bucket] = []
                bucket_replaced.add(bucket)
                bucket_seen[bucket] = set()
                source[bucket] = []
            for it in items:                    # union, deduped, order-kept
                k = it.lower() if isinstance(it, str) else it
                if k not in bucket_seen[bucket]:
                    bucket_seen[bucket].add(k)
                    dice[bucket].append(it)
            if f"{name}.json" not in source[bucket]:
                source[bucket].append(f"{name}.json")
    if missing:
        source["_note"] = (f"no prompt file for: {', '.join(missing)} "
                           f"— those contribute nothing")

    return jsonify({
        "success": True,
        "loras": loras,
        "missing_loras": missing,
        "available_loras": available,
        "prompts": {
            "version": defaults.get("version", 1),
            "dice": dice,
            "source": source,
        },
    })


@app.route("/poll_status/<session_id>", methods=["GET"])
def poll_status(session_id: str):
    """Same JSON shape the gary4juce poller expects (matches foundation-1)."""
    session = get_session(session_id)
    if session is None:
        return jsonify({"success": False, "error": f"unknown session: {session_id}"}), 404

    status = session["status"]

    queue_status = {}
    if status == "queued":
        position = session.get("queue_position", 1) or 1
        estimated_seconds = max(5, position * 20)
        queue_status = {
            "status": "queued",
            "position": position,
            "total_queued": position,
            "message": f"Task queued successfully. You are number {position} in the queue. "
                       f"Estimated wait time: ~{estimated_seconds}s.",
            "estimated_time": f"~{estimated_seconds}s",
            "estimated_seconds": estimated_seconds,
        }
    elif status in ("generating", "encoding"):
        queue_status = {"status": "ready"}

    response = {
        "success": True,
        "generation_in_progress": session["generation_in_progress"],
        "transform_in_progress": session["transform_in_progress"],
        "progress": session["progress"],
        "status": status,
        "queue_status": queue_status,
    }

    if status == "completed":
        response["audio_data"] = session.get("audio_data", "")
        response["meta"] = session.get("meta", {})
    elif status == "failed":
        response["success"] = False
        response["error"] = session.get("error", "unknown error")

    return jsonify(response)


# ---------------------------------------------------------------------------
# Startup
# ---------------------------------------------------------------------------

def warmup():
    try:
        print("Warming up Stable Audio 3...")
        load_pipeline()
        print("Stable Audio 3 ready.")
    except Exception as e:
        print(f"Warmup failed: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    threading.Thread(target=warmup, daemon=True).start()
    port = int(os.environ.get("PORT", 8016))
    print(f"Starting Stable Audio 3 API on port {port}...")
    app.run(host="0.0.0.0", port=port, threaded=True)
