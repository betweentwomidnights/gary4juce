import gevent.monkey
gevent.monkey.patch_all()

import flask
from flask import Flask, jsonify, request, copy_current_request_context, abort
from flask_socketio import SocketIO, emit, join_room, leave_room
from socketio import Client  # Add this import at the top with other imports
import gevent
from pymongo import MongoClient
from gridfs import GridFS
import base64
import redis
from g4laudio import process_audio, continue_music, generate_session_id
from bson.objectid import ObjectId
from pydantic import BaseModel, ValidationError, Field
import torch
from flask_cors import CORS  # Import CORS
import json

from typing import Optional
from datetime import datetime, timezone, timedelta
import requests


import gc
from contextlib import contextmanager

import os
import signal
from weakref import WeakSet
import psutil
import time
from collections import defaultdict
import threading
from utils import parse_client_data

import uuid

# # Add after your existing imports
# websocket_connections = {}  # Track active WebSocket connections
# websocket_lock = threading.Lock()

# def register_websocket_connection(session_id, ws):
#     """Register a WebSocket connection for a session"""
#     with websocket_lock:
#         websocket_connections[session_id] = ws

# def unregister_websocket_connection(session_id):
#     """Unregister a WebSocket connection"""
#     with websocket_lock:
#         websocket_connections.pop(session_id, None)

# def send_to_websocket(session_id, event, data):
#     """Send data to a WebSocket connection"""
#     with websocket_lock:
#         ws = websocket_connections.get(session_id)
#         if ws and not ws.closed:
#             try:
#                 ws.send(json.dumps({'event': event, 'data': data}))
#                 return True
#             except:
#                 websocket_connections.pop(session_id, None)
#     return False

class ThreadManager:
    """Manages active processing threads and ensures cleanup."""
    def __init__(self):
        self.active_threads = WeakSet()
        
    def register_thread(self, greenlet):
        """Register a new thread for tracking."""
        self.active_threads.add(greenlet)
        
    def cleanup_threads(self):
        """Cleanup completed threads and their resources."""
        for thread in list(self.active_threads):
            if thread.dead:
                self.active_threads.remove(thread)

# Create a global thread manager
thread_manager = ThreadManager()

class ProcessTracker:
    """Tracks processes created during audio processing."""
    def __init__(self):
        self.start_pids = set()
        self.current_pid = None
    
    def snapshot_processes(self):
        """Take a snapshot of current Python processes."""
        python_processes = set()
        for proc in psutil.process_iter(['pid', 'name']):
            try:
                if 'python' in proc.info['name'].lower():
                    python_processes.add(proc.info['pid'])
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        return python_processes
    
    def start_tracking(self):
        """Start tracking processes."""
        self.start_pids = self.snapshot_processes()
        self.current_pid = os.getpid()
    
    def cleanup_new_processes(self):
        """Cleanup processes that weren't present when tracking started."""
        current_pids = self.snapshot_processes()
        new_pids = current_pids - self.start_pids
        
        for pid in new_pids:
            if pid != self.current_pid:  # Don't kill our own process
                try:
                    proc = psutil.Process(pid)
                    proc.terminate()
                    try:
                        proc.wait(timeout=3)
                    except psutil.TimeoutExpired:
                        proc.kill()
                except psutil.NoSuchProcess:
                    continue

@contextmanager
def track_processes():
    """Context manager for process tracking."""
    tracker = ProcessTracker()
    tracker.start_tracking()
    try:
        yield
    finally:
        tracker.cleanup_new_processes()
        clean_gpu_memory()

def enhanced_spawn(func):
    """Wrapper for gevent.spawn that ensures proper cleanup."""
    thread = gevent.spawn(func)
    thread_manager.register_thread(thread)
    return thread

@contextmanager
def force_gpu_cleanup():
    """Enhanced GPU cleanup context manager."""
    try:
        yield
    finally:
        # Force release of all GPU memory
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
            # Synchronize all CUDA devices
            torch.cuda.synchronize()
        # Force garbage collection
        gc.collect()

def clean_gpu_memory():
    """Utility function to force GPU memory cleanup."""
    if torch.cuda.is_available():
        # Empty the cache first
        torch.cuda.empty_cache()
        
        # Get all active GPU devices
        devices = range(torch.cuda.device_count())
        for device in devices:
            with torch.cuda.device(device):
                # Synchronize the device to ensure all operations are complete
                torch.cuda.synchronize()
                # Empty the cache again after sync
                torch.cuda.empty_cache()
    
    # Force garbage collection multiple times
    for _ in range(3):
        gc.collect()

# MongoDB setup
# THIS IS THE LOCAL VERSION
# client = MongoClient('mongodb://localhost:27017/')
client = MongoClient('mongodb://mongo:27017/')
db = client['audio_generation_db']
sessions = db.sessions
fs = GridFS(db)

# Redis setup

# THIS IS THE LOCAL VERSION
redis_client = redis.StrictRedis(host='redis', port=6379, db=0)
# redis_client = redis.StrictRedis(host='redis', port=6379, db=0)

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes
# THIS IS THE LOCAL VERSION
socketio = SocketIO(
    app,
    message_queue='redis://redis:6379',
    async_mode='gevent',
    cors_allowed_origins="*",
    logger=True,
    engineio_logger=True,
    ping_timeout=240,  # Use snake_case
    ping_interval=120,  # Use snake_case
    max_http_buffer_size=64*1024*1024,
    ping_timeout_callback=lambda: clean_gpu_memory(),  # Add cleanup on ping timeout
)

@app.route('/')
def index():
    return "The WebSocket server is running."

# Pydantic models for validation
class AudioRequest(BaseModel):
    audio_data: str
    model_name: str
    prompt_duration: int
    top_k: Optional[int] = None
    temperature: Optional[float] = None
    cfg_coef: Optional[float] = None
    description: Optional[str] = None  # New optional field

class SessionRequest(BaseModel):
    session_id: str
    model_name: Optional[str] = None
    prompt_duration: Optional[int] = None
    top_k: Optional[int] = None
    temperature: Optional[float] = None
    cfg_coef: Optional[float] = None
    description: Optional[str] = None  # New optional field

class ContinueMusicRequest(BaseModel):
    session_id: Optional[str] = None  # Make this optional
    model_name: Optional[str] = None
    prompt_duration: Optional[int] = None
    audio_data: Optional[str] = None
    top_k: Optional[int] = None
    temperature: Optional[float] = None
    cfg_coef: Optional[float] = None
    description: Optional[str] = None

class TransformRequest(BaseModel):
    """Pydantic model for transform requests."""
    audio_data: Optional[str] = None  # Optional because we might get it from session
    variation: str
    session_id: Optional[str] = None
    flowstep: Optional[float] = Field(None, ge=0)  # Must be greater than or equal to 0 if provided
    solver: Optional[str] = None  # Optional solver parameter
    custom_prompt: Optional[str] = None  # New optional custom prompt parameter

# Global throttle manager to limit progress update frequency
class ThrottleManager:
    """Manages throttling of frequent events like progress updates."""
    
    def __init__(self):
        self.last_emit_time = defaultdict(float)
        self.min_interval = 1.0  # Minimum seconds between progress updates per session
        self.high_load_interval = 2.0  # Longer interval during high system load
        self.lock = threading.Lock()
        
    def should_emit(self, session_id, event_type, force=False):
        """Determines if an event should be emitted based on throttling rules."""
        now = time.time()
        key = f"{session_id}:{event_type}"
        
        with self.lock:
            # Priority events are never throttled
            if force or event_type in ['queue_status', 'audio_processed', 'music_continued', 'error']:
                self.last_emit_time[key] = now
                return True
            
            # Get active tasks count for adaptive throttling
            active_tasks = self._count_active_tasks()
            
            # Determine throttle interval based on system load
            throttle_interval = self.high_load_interval if active_tasks > 1 else self.min_interval
            
            # Check if enough time has passed since last emission
            last_time = self.last_emit_time.get(key, 0)
            if now - last_time >= throttle_interval:
                self.last_emit_time[key] = now
                return True
                
            return False
    
    def _count_active_tasks(self):
        """Count active generation tasks using Redis."""
        count = 0
        try:
            for key in redis_client.scan_iter("*_generation_in_progress"):
                value = redis_client.get(key)
                if value and value.decode('utf-8') == 'True':
                    count += 1
        except Exception:
            # Default to 1 if we can't determine
            return 1
        return max(count, 1)  # Ensure at least 1

# Create global instance
throttle_manager = ThrottleManager()

# 2. Create an enhanced emit function that applies throttling
def smart_emit(event, data, room=None, namespace=None, force=False):
    """
    Smarter emit function that applies throttling rules and handles errors gracefully.
    
    Args:
        event: The Socket.io event name
        data: The data to emit
        room: The Socket.io room to emit to (optional)
        namespace: The Socket.io namespace (optional)
        force: Whether to bypass throttling (for critical messages)
    """
    session_id = data.get('session_id', room) if isinstance(data, dict) else room

    # For events without a session_id or room, emit directly
    if not session_id:
        try:
            socketio.emit(event, data, room=room, namespace=namespace)
        except Exception as e:
            print(f"[SOCKET] Error emitting {event}: {e}")
        return
    
    # Apply throttling logic
    if not throttle_manager.should_emit(session_id, event, force=force):
        # If throttled, store the event for possible later delivery if it's important
        if event in ['queue_status']:
            # Store in Redis for the worker to pick up
            notification_id = f"{session_id}_{int(time.time())}"
            redis_key = f"throttled_event:{notification_id}"
            event_data = {
                'event': event,
                'data': json.dumps(data),
                'room': session_id,
                'created_at': str(time.time()),
                'throttled': 'true'
            }
            try:
                redis_client.hmset(redis_key, event_data)
                redis_client.expire(redis_key, 30)  # Short expiration
                print(f"[THROTTLE] {event} for {session_id} throttled and stored for later delivery")
            except Exception as e:
                print(f"[THROTTLE] Error storing throttled event: {e}")
                
        return
    
    # Add priority metadata for queue_status messages
    if event == 'queue_status' and isinstance(data, dict):
        if 'priority' not in data:
            data['priority'] = True
    
    # Emit the event
    try:
        socketio.emit(event, data, room=room, namespace=namespace)
        if event != 'progress_update':  # Don't log progress updates to reduce noise
            print(f"[SOCKET] Emitted {event} to {session_id}")
    except Exception as e:
        print(f"[SOCKET] Error emitting {event} to {session_id}: {e}")
        
        # If this was an important event, store for retry by worker
        if event in ['queue_status', 'audio_processed', 'music_continued', 'error']:
            notification_id = f"{session_id}_{int(time.time())}"
            redis_key = f"failed_event:{notification_id}"
            event_data = {
                'event': event,
                'data': json.dumps(data) if isinstance(data, dict) else str(data),
                'room': session_id,
                'created_at': str(time.time()),
                'error': str(e)
            }
            try:
                redis_client.hmset(redis_key, event_data)
                redis_client.expire(redis_key, 3600)  # 1 hour expiration
            except Exception as redis_err:
                print(f"[SOCKET] Error storing failed event: {redis_err}")

class NotificationWorker:
    """
    Dedicated worker for handling queue notifications without blocking the main process.
    """
    def __init__(self, socketio_instance, redis_client_instance):
        self.socketio = socketio_instance
        self.redis_client = redis_client_instance
        self.worker_greenlet = None
        self.running = False
    
    def start(self):
        """Start the notification worker thread."""
        if self.worker_greenlet is None or self.worker_greenlet.dead:
            self.running = True
            self.worker_greenlet = gevent.spawn(self._worker_loop)
            print("[WORKER] Notification worker started")
        return self.worker_greenlet
    
    def stop(self):
        """Stop the notification worker thread."""
        self.running = False
        if self.worker_greenlet and not self.worker_greenlet.dead:
            gevent.kill(self.worker_greenlet)
            print("[WORKER] Notification worker stopped")
    
    def _process_failed_events(self):
        """Process events that failed to emit."""
        failed_keys = list(self.redis_client.scan_iter("failed_event:*"))
        for key in failed_keys[:5]:  # Process up to 5 per cycle
            try:
                key_str = key.decode('utf-8') if isinstance(key, bytes) else key
                data = self.redis_client.hgetall(key_str)
                if not data:
                    continue
                    
                # Get event data
                event = data.get(b'event', b'').decode('utf-8')
                event_data_json = data.get(b'data', b'{}').decode('utf-8')
                room = data.get(b'room', b'').decode('utf-8')
                
                if not event or not room:
                    self.redis_client.delete(key_str)
                    continue
                    
                try:
                    event_data = json.loads(event_data_json)
                except:
                    event_data = event_data_json
                    
                # Try to emit the event
                try:
                    self.socketio.emit(event, event_data, room=room, namespace='/')
                    print(f"[WORKER] Successfully re-emitted failed {event} to {room}")
                    self.redis_client.delete(key_str)
                except Exception as e:
                    print(f"[WORKER] Failed to re-emit {event} to {room}: {e}")
                    # Update retry count
                    retry_count = int(data.get(b'retry_count', b'0').decode('utf-8')) + 1
                    if retry_count >= 5:
                        self.redis_client.delete(key_str)
                    else:
                        self.redis_client.hset(key_str, 'retry_count', str(retry_count))
            except Exception as e:
                print(f"[WORKER] Error processing failed event {key}: {e}")

    def _process_throttled_events(self):
        """Process events that were throttled."""
        throttled_keys = list(self.redis_client.scan_iter("throttled_event:*"))
        for key in throttled_keys[:5]:  # Process up to 5 per cycle
            try:
                key_str = key.decode('utf-8') if isinstance(key, bytes) else key
                data = self.redis_client.hgetall(key_str)
                if not data:
                    continue
                    
                # Get event data
                event = data.get(b'event', b'').decode('utf-8')
                event_data_json = data.get(b'data', b'{}').decode('utf-8')
                room = data.get(b'room', b'').decode('utf-8')
                
                if not event or not room:
                    self.redis_client.delete(key_str)
                    continue
                    
                try:
                    event_data = json.loads(event_data_json)
                except:
                    event_data = event_data_json
                    
                # Try to emit the event
                try:
                    self.socketio.emit(event, event_data, room=room, namespace='/')
                    print(f"[WORKER] Successfully emitted throttled {event} to {room}")
                    self.redis_client.delete(key_str)
                except Exception as e:
                    print(f"[WORKER] Failed to emit throttled {event} to {room}: {e}")
            except Exception as e:
                print(f"[WORKER] Error processing throttled event {key}: {e}")
    
    def _worker_loop(self):
        """Main worker loop that processes pending notifications."""
        while self.running:
            try:
                # First process any failed events (higher priority)
                self._process_failed_events()
                
                # Process throttled events if system load is low enough
                active_tasks = throttle_manager._count_active_tasks()
                if active_tasks <= 1:
                    self._process_throttled_events()
                
                # Process normal notifications (original code)
                pending_keys = list(self.redis_client.scan_iter("task_notification:*"))
                
                for key in pending_keys:
                    try:
                        key_str = key.decode('utf-8') if isinstance(key, bytes) else key
                        
                        # Get notification data
                        notification_data = self.redis_client.hgetall(key_str)
                        if not notification_data:
                            continue
                            
                        # Check if it's been processed
                        processed = notification_data.get(b'processed', b'false').decode('utf-8')
                        if processed == 'true':
                            # Already processed, skip
                            continue
                            
                        # Check if it's been processed by worker
                        worker_processed = notification_data.get(b'worker_processed', b'false').decode('utf-8')
                        if worker_processed == 'true':
                            # Already attempted by worker, skip to avoid duplicate attempts
                            continue
                            
                        # Check if it's a recent notification (within 30 seconds)
                        received_at = float(notification_data.get(b'received_at', b'0').decode('utf-8'))
                        if time.time() - received_at > 30:
                            # Old notification, mark as expired
                            self.redis_client.hset(key_str, 'worker_processed', 'expired')
                            continue
                        
                        # Extract session_id from the key
                        # Format: task_notification:{session_id}_{timestamp}
                        parts = key_str.split(':')
                        if len(parts) < 2:
                            continue
                            
                        session_parts = parts[1].split('_')
                        if len(session_parts) < 2:
                            continue
                            
                        session_id = session_parts[0]
                        
                        # Mark as being processed by worker
                        self.redis_client.hset(key_str, 'worker_processed', 'true')
                        self.redis_client.hset(key_str, 'worker_started_at', str(time.time()))
                        
                        # Get JSON data
                        data_json = notification_data.get(b'data', b'{}').decode('utf-8')
                        
                        # Process notification
                        try:
                            data = json.loads(data_json)
                            status = data.get('status')
                            queue_status = data.get('queue_status', {})
                            
                            print(f"[WORKER] Processing notification for {session_id}: {status}")
                            
                            # Ensure queue_status has the status info
                            if isinstance(queue_status, dict):
                                queue_status['status'] = status
                            
                            # Generate status message
                            status_message = get_queue_status_message({'queue_status': queue_status})
                            status_message['session_id'] = session_id
                            status_message['notification_id'] = parts[1]
                            status_message['from_worker'] = True
                            
                            # Use socket event with force=True to bypass throttling for queue status messages
                            smart_emit('queue_status', status_message, room=session_id, force=True)
                            
                            # Mark success
                            self.redis_client.hset(key_str, 'worker_success', 'true')
                            self.redis_client.hset(key_str, 'processed', 'true')
                            print(f"[WORKER] Successfully emitted queue_status to {session_id}")
                            
                        except Exception as e:
                            print(f"[WORKER] Error processing notification {key_str}: {e}")
                            self.redis_client.hset(key_str, 'worker_error', str(e))
                            self.redis_client.hset(key_str, 'worker_success', 'false')
                            
                    except Exception as e:
                        print(f"[WORKER] Error handling notification key {key}: {e}")
                
                # Sleep to prevent CPU hogging
                gevent.sleep(0.2)  # Shorter sleep for better responsiveness
                
            except Exception as e:
                print(f"[WORKER] Error in notification worker loop: {e}")
                gevent.sleep(1)  # Longer sleep on error



# Initialization code to add to your main.py after socketio setup:
def initialize_workers():
    """Initialize and start background workers."""
    global notification_worker
    
    # Create the notification worker
    notification_worker = NotificationWorker(socketio, redis_client)
    notification_worker.start()
    
    # Log worker initialization
    print("[INIT] Started background workers")
    
    return notification_worker

# Add this to your shutdown/cleanup logic to ensure workers are stopped
def cleanup_workers():
    """Stop all background workers."""
    if 'notification_worker' in globals() and notification_worker:
        notification_worker.stop()
        print("[CLEANUP] Notification worker stopped")

# This should be initialized after your socketio setup
notification_worker = None

# After creating socketio:
notification_worker = initialize_workers()


def store_audio_in_gridfs(data, filename):
    """Store audio data in GridFS."""
    audio_data = base64.b64decode(data)
    file_id = fs.put(audio_data, filename=filename)
    return str(file_id)

def retrieve_audio_from_gridfs(file_id):
    """Retrieve audio data from GridFS."""
    try:
        file = fs.get(ObjectId(file_id))
        return base64.b64encode(file.read()).decode('utf-8')
    except Exception as e:
        print(f"Error retrieving audio from GridFS: {e}")
        return None

def store_audio_data(session_id, audio_data, key):
    """Store session data in MongoDB with GridFS."""
    file_id = store_audio_in_gridfs(audio_data, f"{session_id}_{key}.wav")
    current_time = datetime.now(timezone.utc)
    sessions.update_one(
        {'_id': session_id},
        {
            '$set': {
                key: file_id,
                'updated_at': current_time
            },
            '$setOnInsert': {
                'created_at': current_time
            }
        },
        upsert=True
    )

def retrieve_audio_data(session_id, key):
    """Retrieve specific audio data from MongoDB."""
    session_data = sessions.find_one({'_id': session_id})
    file_id = session_data.get(key) if session_data else None
    return retrieve_audio_from_gridfs(file_id) if file_id else None

def set_generation_in_progress(session_id, in_progress):
    """Set or unset the generation_in_progress flag in Redis."""
    redis_client.set(f"{session_id}_generation_in_progress", str(in_progress))

def format_time_estimate(seconds):
    """Convert seconds to a human-readable format"""
    if seconds < 60:
        return f"about {int(seconds)} seconds"
    minutes = int(seconds / 60)
    remaining_seconds = int(seconds % 60)
    if remaining_seconds == 0:
        return f"about {minutes} minute{'s' if minutes != 1 else ''}"
    return f"about {minutes} minute{'s' if minutes != 1 else ''} and {remaining_seconds} seconds"

def get_queue_status_message(queue_response):
    """Generate a user-friendly queue status message with simplified time estimates"""
    status = queue_response.get('queue_status', {})
    if not status and isinstance(queue_response, dict):
        status = queue_response
        
    print(f"Processing status data: {status}")  # Debug log
    
    estimated_wait = float(status.get('estimated_wait_seconds', 30.0))
    active_tasks = int(status.get('active_tasks', 0))
    queued_tasks = int(status.get('queued_tasks', 0))
    task_status = status.get('status', '')
    
    print(f"Parsed metrics - Wait: {estimated_wait}s, Active: {active_tasks}, " 
          f"Queued: {queued_tasks}, Status: {task_status}")  # Debug log

    if task_status == 'ready':
        message = "Starting generation now..."
    elif task_status == 'queued':
        message = (f"Task queued successfully. You are number {queued_tasks} in the queue. "
                  f"Estimated wait time: {format_time_estimate(estimated_wait)}.")
    else:
        message = "Request received, determining queue status..."

    return {
        "message": message,
        "position": queued_tasks,
        "total_queued": queued_tasks + active_tasks,
        "estimated_time": format_time_estimate(estimated_wait),
        "estimated_seconds": estimated_wait,
        "status": task_status
    }

def queue_task(session_id: str, task_type: str, task_data: dict) -> dict:
    """Queue a task with the Go service and return the response"""
    try:
        task_payload = {
            'session_id': session_id,
            'task_type': task_type,
            'data': json.dumps({
                'model_name': task_data.get('model_name'),
                'prompt_duration': task_data.get('prompt_duration'),
                'top_k': task_data.get('top_k', 250),
                'temperature': task_data.get('temperature', 1.0),
                'cfg_coef': task_data.get('cfg_coef', 3.0),
                'description': task_data.get('description'),
            })
        }

        print(f"Sending task to queue service: {session_id}")
        response = requests.post(
            'http://gpu-queue:8085/tasks',
            json=task_payload,
            headers={'Content-Type': 'application/json'}
        )
        response.raise_for_status()
        
        response_data = response.json()
        print(f"Queue response for {session_id}: {response_data}")  # Debug log
        return response_data
    except requests.exceptions.RequestException as e:
        print(f"Error queuing task: {e}")
        return None

def is_generation_in_progress(session_id: str) -> bool:
    """Modified to check with Go queue service"""
    try:
        response = requests.get(f'http://gpu-queue:8085/tasks/{session_id}')
        if response.status_code == 200:
            task_status = response.json()
            return task_status.get('status') in ['processing', 'queued']
        return False
    except Exception as e:
        print(f"Error checking task status: {e}")
        return False

def queue_transform_task(session_id: str, task_type: str, task_data: dict) -> dict:
    """Queue a transform task with the Go service and return the response"""
    try:
        task_payload = {
            'session_id': session_id,
            'task_type': task_type,
            'data': json.dumps(task_data)
        }

        print(f"Sending transform task to queue service: {session_id}")
        response = requests.post(
            'http://gpu-queue:8085/transform/tasks',
            json=task_payload,
            headers={'Content-Type': 'application/json'}
        )
        response.raise_for_status()
        
        response_data = response.json()
        print(f"Transform queue response for {session_id}: {response_data}")
        return response_data
    except requests.exceptions.RequestException as e:
        print(f"Error queuing transform task: {e}")
        return None

def is_transform_in_progress(session_id: str) -> bool:
    """Check if a transform is in progress for the given session"""
    try:
        # First check Redis flag (faster than HTTP request)
        redis_value = redis_client.get(f"{session_id}_transform_in_progress")
        if redis_value and redis_value.decode('utf-8') == 'True':
            return True
            
        # Then check with Go queue service
        response = requests.get(f'http://gpu-queue:8085/transform/tasks/{session_id}')
        if response.status_code == 200:
            task_status = response.json()
            return task_status.get('status') in ['processing', 'queued']
        return False
    except Exception as e:
        print(f"Error checking transform task status: {e}")
        return False

def set_transform_in_progress(session_id, in_progress):
    """Set or unset the transform_in_progress flag in Redis."""
    redis_client.set(f"{session_id}_transform_in_progress", str(in_progress))
    redis_client.expire(f"{session_id}_transform_in_progress", 3600)  # 1 hour expiration

def store_transform_task_data(session_id, task_data):
    """Store transform task data in Redis."""
    redis_client.set(f"transform_task:{session_id}:data", json.dumps(task_data))
    redis_client.expire(f"transform_task:{session_id}:data", 3600)  # 1 hour expiration

@socketio.on('cleanup_session_request')
def handle_cleanup_request(data):
    try:
        request_data = SessionRequest(**data)
        session_id = request_data.session_id
        if session_id:
            with track_processes():  # Track and clean up processes
                with force_gpu_cleanup():
                    # Clean up Redis
                    redis_client.delete(f"{session_id}_generation_in_progress")
                    
                    # Clear any chunked data
                    chunk_pattern = f"{session_id}_chunk_*"
                    for key in redis_client.scan_iter(chunk_pattern):
                        redis_client.delete(key)
                    redis_client.delete(f"{session_id}_received_chunks_set")
                    
                    # Leave the socket room
                    leave_room(session_id)
                    
                    # Force GPU memory cleanup
                    clean_gpu_memory()
                    
                    # Note: We're keeping the MongoDB session data for restore capabilities
                    # but we ensure all GPU resources are freed
                    
                    # Clean up any remaining threads
                    thread_manager.cleanup_threads()
                    
                    emit('cleanup_complete', {
                        'message': 'Session cleaned up, GPU memory freed, and processes terminated',
                        'session_id': session_id
                    }, room=session_id)
                    
    except ValidationError as e:
        emit('error', {'message': str(e), 'session_id': data.get('session_id')})
    finally:
        # One final cleanup pass
        clean_gpu_memory()
        thread_manager.cleanup_threads()

def retry_socket_emit(event, data, room, max_attempts=5, base_delay=0.1):
    """
    Robust socket.io event emission with exponential backoff and guaranteed delivery.
    """
    attempt = 0
    last_error = None
    
    # Create a unique message ID for tracking this emission
    message_id = f"{room}_{int(time.time())}_{event}_{hash(str(data))}"
    redis_tracking_key = f"socket_emit_tracking:{message_id}"
    
    # Store attempt information in Redis for monitoring and diagnostics
    # USE HMSET INSTEAD OF SETEX to ensure we're using a hash consistently
    initial_data = {
        'event': event,
        'room': room,
        'attempts': '0',
        'status': 'pending',
        'created_at': str(time.time())
    }
    redis_client.hmset(redis_tracking_key, initial_data)
    redis_client.expire(redis_tracking_key, 3600)  # Set expiration separately
    
    print(f"[SOCKET] Attempting to emit {event} to room {room} (tracking ID: {message_id})")
    
    while attempt < max_attempts:
        try:
            # Track current attempt - now correctly using a hash
            redis_client.hset(redis_tracking_key, 'attempts', str(attempt + 1))
            
            # Force-join the room again to ensure connection
            if hasattr(flask.request, 'sid'):
                try:
                    join_room(room)
                    print(f"[SOCKET] Re-joined room {room} for guaranteed delivery")
                except Exception as room_err:
                    print(f"[SOCKET] Could not re-join room {room}: {room_err}")
            
            # Ensure the socketio event has a tracking ID
            if isinstance(data, dict):
                data['_tracking_id'] = message_id
            
            # Emit with acknowledgment callback if possible
            socketio.emit(event, data, room=room, callback=lambda ack: 
                redis_client.hset(redis_tracking_key, 'ack_received', 'true') 
                if ack else None)
            
            print(f"[SOCKET] Successfully emitted {event} to {room} (attempt {attempt+1}/{max_attempts})")
            redis_client.hset(redis_tracking_key, 'status', 'success')
            return True
            
        except Exception as e:
            last_error = e
            attempt += 1
            delay = base_delay * (2 ** attempt)  # Exponential backoff
            
            print(f"[SOCKET] Error emitting {event} to {room}, attempt {attempt}/{max_attempts}: {e}")
            redis_client.hset(redis_tracking_key, 'last_error', str(e))
            
            # Don't retry immediately - allow other operations to proceed
            time.sleep(delay)
    
    error_msg = f"Failed to emit {event} after {max_attempts} attempts: {last_error}"
    print(f"[SOCKET ERROR] {error_msg}")
    redis_client.hset(redis_tracking_key, 'status', 'failed')
    redis_client.hset(redis_tracking_key, 'error', error_msg)
    
    return False

# Add these helper functions to store progress/results for HTTP clients:

def store_session_progress(session_id, progress):
    """Store progress for HTTP polling clients"""
    try:
        redis_client.setex(f"progress:{session_id}", 3600, str(progress))  # 1 hour TTL
    except:
        pass

def store_session_status(session_id, status, error_message=None):
    """Store status for HTTP polling clients"""
    try:
        status_data = {
            'status': status,
            'timestamp': time.time()
        }
        if error_message:
            status_data['error'] = error_message
            
        redis_client.setex(f"status:{session_id}", 3600, json.dumps(status_data))
    except:
        pass

def get_session_progress(session_id):
    """Get progress for HTTP polling clients"""
    try:
        progress = redis_client.get(f"progress:{session_id}")
        return int(progress) if progress else 0
    except:
        return 0

def get_session_status(session_id):
    """Get status for HTTP polling clients"""
    try:
        status_data = redis_client.get(f"status:{session_id}")
        return json.loads(status_data) if status_data else {'status': 'unknown'}
    except:
        return {'status': 'unknown'}

@app.route('/task_notification', methods=['POST'])
def handle_task_notification():
    """
    Handle task notifications from the Go queue service with improved reliability.
    This implementation minimizes blocking during active tasks.
    """
    try:
        data = request.json
        if not data:
            return jsonify({'status': 'error', 'message': 'No JSON data received'}), 400
            
        session_id = data.get('session_id')
        if not session_id:
            return jsonify({'status': 'error', 'message': 'Missing session_id'}), 400
            
        status = data.get('status')
        task_type = data.get('type')
        
        # Get notification ID from headers if available or generate a new one
        notification_id = request.headers.get('X-Notification-ID', f"{session_id}_{int(time.time())}")
        redis_key = f"task_notification:{notification_id}"
        
        # Store notification in Redis immediately
        notification_data = {
            'data': json.dumps(data),
            'received_at': str(time.time()),
            'processed': 'false',
            'worker_processed': 'false',
            'headers': json.dumps(dict(request.headers)),
            'urgent': 'true' if status == 'ready' else 'false'
        }
        
        redis_client.hmset(redis_key, notification_data)
        redis_client.expire(redis_key, 3600)  # 1 hour expiration
        
        print(f"[QUEUE] Stored notification {notification_id} for {session_id} with status {status}")
        
        # For 'ready' status, start task processing right away
        if status == 'ready':
            redis_client.hset(redis_key, 'processing_started', 'true')
            
            # Launch the appropriate task handler in a background job
            if task_type == 'process_audio':
                thread = enhanced_spawn(lambda: handle_task_ready(session_id))
                print(f"[QUEUE] Started handle_task_ready for session {session_id}")
            elif task_type == 'continue_music':
                thread = enhanced_spawn(lambda: handle_task_ready_continue(session_id))
                print(f"[QUEUE] Started handle_task_ready_continue for session {session_id}")
            elif task_type == 'retry_music':
                thread = enhanced_spawn(lambda: handle_task_ready_retry(session_id))
                print(f"[QUEUE] Started handle_task_ready_retry for session {session_id}")
            elif task_type == 'transform_audio':
                thread = enhanced_spawn(lambda: handle_task_ready_transform(session_id))
                print(f"[QUEUE] Started handle_task_ready_transform for session {session_id}")
        
        # Try immediate emit while we have request context
        try:
            queue_status = data.get('queue_status', {})
            if isinstance(queue_status, dict):
                queue_status['status'] = status
            
            # Generate the status message
            status_message = get_queue_status_message({'queue_status': queue_status})
            status_message['session_id'] = session_id
            status_message['notification_id'] = notification_id
            
            # Since we are in a request context, we can emit directly
            smart_emit('queue_status', status_message, room=session_id, force=True)
            
            # Mark as processed since we succeeded
            redis_client.hset(redis_key, 'immediate_emit', 'true')
            redis_client.hset(redis_key, 'processed', 'true')
            
            print(f"[QUEUE] Successfully emitted queue_status for {session_id} in request handler")
            
        except Exception as emit_error:
            # Just log the error - the worker will handle it
            print(f"[WARN] Initial emit failed, worker will retry: {emit_error}")
            redis_client.hset(redis_key, 'immediate_emit_error', str(emit_error))
        
        # Return success response
        return jsonify({
            'status': 'received',
            'notification_id': notification_id,
            'message': 'Task notification received for processing'
        })
        
    except Exception as e:
        # Catch-all exception handler
        error_id = f"error_{int(time.time())}"
        print(f"[ERROR] {error_id} in handle_task_notification: {str(e)}")
        
        # Return error response
        return jsonify({
            'status': 'error',
            'error_id': error_id,
            'message': f'Exception processing task notification: {str(e)}'
        }), 500


@socketio.on('process_audio_request')
def handle_audio_processing(data):
    try:
        # Basic cleanup
        clean_gpu_memory()
        thread_manager.cleanup_threads()
        
        # Parse and clean client data
        try:
            data = parse_client_data(data)
        except ValueError as e:
            emit('error', {'message': str(e)})
            return

        # Validate request
        request_data = AudioRequest(**data)
        session_id = generate_session_id()

        # Add debug logging for MongoDB storage
        mongo_data = {
            '_id': session_id,  # Make sure we use same ID
            'model_name': str(request_data.model_name),
            'prompt_duration': int(request_data.prompt_duration),
            'parameters': {  # Group optional parameters
                'top_k': int(request_data.top_k) if request_data.top_k is not None else 250,
                'temperature': float(request_data.temperature) if request_data.temperature is not None else 1.0,
                'cfg_coef': float(request_data.cfg_coef) if request_data.cfg_coef is not None else 3.0,
            },
            'description': request_data.description,
            'created_at': datetime.now(timezone.utc)
        }
        
        print(f"Storing session data for {session_id}: {mongo_data}")  # Debug log
        
        sessions.update_one(
            {'_id': session_id},
            {'$set': mongo_data},
            upsert=True
        )

        # Store initial audio data
        store_audio_data(session_id, request_data.audio_data, 'initial_audio')

        # Join room first
        join_room(session_id)

        # Verify room joining by setting a Redis flag using hash instead of string
        room_key = f"room_joined:{session_id}"
        redis_client.hmset(room_key, {'joined': 'true', 'timestamp': str(time.time())})
        redis_client.expire(room_key, 3600)  # 1 hour expiration
        
        # Emit initial acknowledgment without queue status
        emit('process_audio_received', {'session_id': session_id})
        print(f"Emitted process_audio_received for session {session_id}")

        # Queue the task
        queue_response = queue_task(session_id, 'process_audio', data)
        if not queue_response:
            emit('error', {'message': 'Failed to queue task', 'session_id': session_id})
            return

        # We don't emit queue status here anymore - we'll let the task_notification handle it

    except ValidationError as e:
        emit('error', {'message': str(e), 'session_id': generate_session_id()})
    finally:
        thread_manager.cleanup_threads()

# Modify the existing handle_task_ready function (BACKWARDS COMPATIBLE):
def handle_task_ready(session_id):
    try:
        print(f"[DEBUG] Starting handle_task_ready for {session_id}")
        
        session_data = sessions.find_one({'_id': session_id})
        print(f"[DEBUG] Retrieved session data keys: {list(session_data.keys()) if session_data else 'None'}")
        
        if not session_data:
            print(f"[ERROR] No session data found for {session_id}")
            raise ValueError(f"No session data found for {session_id}")

        # Verify required fields with types
        try:
            model_name = str(session_data['model_name'])
            print(f"[DEBUG] Model name: {model_name}")
        except Exception as e:
            print(f"[ERROR] Failed to get model_name: {e}")
            raise
            
        try:
            prompt_duration = int(session_data['prompt_duration'])
            print(f"[DEBUG] Prompt duration: {prompt_duration}")
        except Exception as e:
            print(f"[ERROR] Failed to get prompt_duration: {e}")
            raise
            
        parameters = session_data.get('parameters', {})
        print(f"[DEBUG] Parameters: {parameters}")
        
        # Get audio and verify
        try:
            input_data_base64 = retrieve_audio_data(session_id, 'initial_audio')
            print(f"[DEBUG] Retrieved audio data length: {len(input_data_base64) if input_data_base64 else 'None'}")
        except Exception as e:
            print(f"[ERROR] Failed to retrieve audio data: {e}")
            raise
            
        if not input_data_base64:
            print(f"[ERROR] No audio data found for {session_id}")
            raise ValueError(f"No audio data found for {session_id}")

        # Mark as processing (both for websocket and HTTP clients)
        set_generation_in_progress(session_id, True)
        store_session_status(session_id, 'processing')  # NEW: For HTTP clients

         # Notify Go service that task is now processing
        requests.post(
            'http://gpu-queue:8085/task/status',
            json={
                'session_id': session_id,
                'status': 'processing'
            }
        )

        def audio_processing_thread():
            try:
                with track_processes():
                    def progress_callback(current, total):
                        progress_percent = (current / total) * 100
                        
                        # EXISTING: Emit for websocket clients
                        smart_emit('progress_update',
                            {'progress': int(progress_percent), 'session_id': session_id}, 
                            room=session_id)
                        
                        # NEW: Store for HTTP clients
                        store_session_progress(session_id, int(progress_percent))

                    # Use our verified parameters
                    result_base64 = process_audio(
                        input_data_base64,
                        model_name,  # Use verified model_name
                        progress_callback,
                        prompt_duration=prompt_duration,  # Use verified prompt_duration
                        top_k=parameters.get('top_k', 250),
                        temperature=parameters.get('temperature', 1.0),
                        cfg_coef=parameters.get('cfg_coef', 3.0),
                        description=session_data.get('description')
                    )
                    
                    # Store result (EXISTING - works for both websocket and HTTP)
                    store_audio_data(session_id, result_base64, 'last_processed_audio')
                    
                    # EXISTING: Emit for websocket clients
                    socketio.emit('audio_processed',
                         {'audio_data': result_base64, 'session_id': session_id}, 
                         room=session_id)
                    
                    # NEW: Store completion status for HTTP clients
                    store_session_status(session_id, 'completed')

                    # Notify Go service that task is complete
                    requests.post(
                        'http://gpu-queue:8085/task/status',
                        json={
                            'session_id': session_id,
                            'status': 'completed'
                        }
                    )

            except Exception as e:
                print(f"Error during audio processing thread for session {session_id}: {e}")
                
                # EXISTING: Emit for websocket clients
                socketio.emit('error', {'message': str(e), 'session_id': session_id}, room=session_id)
                
                # NEW: Store error for HTTP clients
                store_session_status(session_id, 'failed', str(e))
                
                # Notify Go service of failure
                requests.post(
                    'http://gpu-queue:8085/task/status',
                    json={
                        'session_id': session_id,
                        'status': 'failed'
                    }
                )
            finally:
                set_generation_in_progress(session_id, False)
                clean_gpu_memory()
                thread_manager.cleanup_threads()

        enhanced_spawn(audio_processing_thread)

    except Exception as e:
        print(f"Error in handle_task_ready for session {session_id}: {e}")
        
        # EXISTING: Emit for websocket clients
        socketio.emit('error', {
            'message': str(e),
            'session_id': session_id
        }, room=session_id)
        
        # NEW: Store error for HTTP clients
        store_session_status(session_id, 'failed', str(e))
        
        set_generation_in_progress(session_id, False)

@socketio.on('continue_music_request')
def handle_continue_music(data):
    try:
        clean_gpu_memory()
        thread_manager.cleanup_threads()
        
        try:
            data = parse_client_data(data)
        except ValueError as e:
            emit('error', {'message': str(e)})
            return

        request_data = ContinueMusicRequest(**data)
        session_id = request_data.session_id

        # NEW: Auto-create session if none exists and audio_data provided
        if not session_id and request_data.audio_data:
            session_id = generate_session_id()
            print(f"Auto-creating session {session_id} for continue request")
            
            # Create minimal session with defaults (similar to crop logic)
            mongo_data = {
                '_id': session_id,
                'model_name': request_data.model_name or 'thepatch/vanya_ai_dnb_0.1',
                'prompt_duration': request_data.prompt_duration or 6,
                'parameters': {
                    'top_k': int(request_data.top_k) if request_data.top_k else 250,
                    'temperature': float(request_data.temperature) if request_data.temperature else 1.0,
                    'cfg_coef': float(request_data.cfg_coef) if request_data.cfg_coef else 3.0,
                },
                'description': request_data.description,
                'created_at': datetime.now(timezone.utc),
                'created_from': 'jerry_continue'  # Flag to indicate origin
            }
            
            sessions.update_one(
                {'_id': session_id},
                {'$set': mongo_data},
                upsert=True
            )
            
            # Store the audio data as last_processed_audio
            store_audio_data(session_id, request_data.audio_data, 'last_processed_audio')
            
        elif not session_id:
            # No session and no audio data - this is an error
            emit('error', {'message': 'No session ID provided and no audio data to create session'})
            return

        if is_generation_in_progress(session_id):
            emit('error', {'message': 'Generation already in progress', 'session_id': session_id}, room=session_id)
            return

        # Extract parameters for MongoDB storage
        top_k = int(request_data.top_k) if request_data.top_k is not None else 250
        temperature = float(request_data.temperature) if request_data.temperature is not None else 1.0
        cfg_coef = float(request_data.cfg_coef) if request_data.cfg_coef is not None else 3.0
        description = request_data.description

        # Get or set model parameters
        model_name = request_data.model_name or sessions.find_one({'_id': session_id}).get('model_name')
        prompt_duration = request_data.prompt_duration or sessions.find_one({'_id': session_id}).get('prompt_duration')

        # Update session data in MongoDB
        mongo_data = {
            'model_name': model_name,
            'prompt_duration': prompt_duration,
            'parameters': {
                'top_k': top_k,
                'temperature': temperature,
                'cfg_coef': cfg_coef,
            },
            'description': description,
            'updated_at': datetime.now(timezone.utc)
        }
        
        sessions.update_one(
            {'_id': session_id},
            {'$set': mongo_data},
            upsert=True
        )

        # Store audio data if provided in request
        if request_data.audio_data:
            store_audio_data(session_id, request_data.audio_data, 'last_processed_audio')

        # Prepare task data
        task_data = {
            'model_name': model_name,
            'prompt_duration': prompt_duration,
            'top_k': top_k,
            'temperature': temperature,
            'cfg_coef': cfg_coef,
            'description': description
        }
        
        # Queue the task
        queue_response = queue_task(session_id, 'continue_music', task_data)
        if not queue_response:
            emit('error', {'message': 'Failed to queue task', 'session_id': session_id})
            return

        # Join room for this session
        join_room(session_id)

        # Emit queue status to client
        # Emit queue status to client with time estimates
        # queue_status = get_queue_status_message(queue_response)
        # queue_status['session_id'] = session_id  # Add session_id to the response
        # emit('queue_status', queue_status)

        # Emit continue music received confirmation
        emit('continue_music_received', {'session_id': session_id})
        print(f"Emitted continue_music_received for session {session_id}")

    except ValidationError as e:
        emit('error', {'message': str(e), 'session_id': data.get('session_id') if isinstance(data, dict) else None})
    finally:
        thread_manager.cleanup_threads()

def handle_task_ready_continue(session_id):
    try:
        session_data = sessions.find_one({'_id': session_id})
        print(f"Retrieved session data for continue music {session_id}: {session_data}")
        
        if not session_data:
            raise ValueError(f"No session data found for {session_id}")

        # Get audio data based on what was stored - either from request or last processed
        input_data_base64 = retrieve_audio_data(session_id, 'last_processed_audio')
        if not input_data_base64:
            raise ValueError(f"No audio data found for {session_id}")

        # Verify required fields with types
        model_name = str(session_data['model_name'])
        prompt_duration = int(session_data['prompt_duration'])
        parameters = session_data.get('parameters', {})
        
        # Mark as processing (both for websocket and HTTP clients)
        set_generation_in_progress(session_id, True)
        store_session_status(session_id, 'processing')  # NEW: For HTTP clients

         # Notify Go service that task is now processing
        requests.post(
            'http://gpu-queue:8085/task/status',
            json={
                'session_id': session_id,
                'status': 'processing'
            }
        )

        def continue_music_processing_thread():
            try:
                with track_processes():
                    def progress_callback(current, total):
                        progress_percent = (current / total) * 100
                        
                        # EXISTING: Emit for websocket clients
                        smart_emit('progress_update',
                             {'progress': int(progress_percent), 'session_id': session_id}, 
                             room=session_id)
                        
                        # NEW: Store for HTTP clients
                        store_session_progress(session_id, int(progress_percent))

                    # Use verified parameters
                    result_base64 = continue_music(
                        input_data_base64,
                        model_name,
                        progress_callback,
                        prompt_duration=prompt_duration,
                        top_k=parameters.get('top_k', 250),
                        temperature=parameters.get('temperature', 1.0),
                        cfg_coef=parameters.get('cfg_coef', 3.0),
                        description=session_data.get('description')
                    )
                    
                    # Store results (EXISTING - works for both websocket and HTTP)
                    store_audio_data(session_id, input_data_base64, 'last_input_audio')
                    store_audio_data(session_id, result_base64, 'last_processed_audio')
                    
                    # EXISTING: Emit for websocket clients
                    socketio.emit('music_continued',
                         {'audio_data': result_base64, 'session_id': session_id}, 
                         room=session_id)
                    
                    # NEW: Store completion status for HTTP clients
                    store_session_status(session_id, 'completed')

                    # Notify Go service that task is complete
                    requests.post(
                        'http://gpu-queue:8085/task/status',
                        json={
                            'session_id': session_id,
                            'status': 'completed'
                        }
                    )

            except Exception as e:
                print(f"Error during continue music processing thread for session {session_id}: {e}")
                
                # EXISTING: Emit for websocket clients
                socketio.emit('error', {'message': str(e), 'session_id': session_id}, room=session_id)
                
                # NEW: Store error for HTTP clients
                store_session_status(session_id, 'failed', str(e))
                
                # Notify Go service of failure
                requests.post(
                    'http://gpu-queue:8085/task/status',
                    json={
                        'session_id': session_id,
                        'status': 'failed'
                    }
                )
            finally:
                set_generation_in_progress(session_id, False)
                clean_gpu_memory()
                thread_manager.cleanup_threads()

        enhanced_spawn(continue_music_processing_thread)

    except Exception as e:
        print(f"Error in handle_task_ready_continue for session {session_id}: {e}")
        
        # EXISTING: Emit for websocket clients
        socketio.emit('error', {
            'message': str(e),
            'session_id': session_id
        }, room=session_id)
        
        # NEW: Store error for HTTP clients  
        store_session_status(session_id, 'failed', str(e))
        
        set_generation_in_progress(session_id, False)

@socketio.on('retry_music_request')
def handle_retry_music(data):
    try:
        clean_gpu_memory()
        thread_manager.cleanup_threads()
        
        # Handle string data (from Swift)
        
        try:
            data = parse_client_data(data)
        except ValueError as e:
            emit('error', {'message': str(e)})
            return

        request_data = SessionRequest(**data)
        session_id = request_data.session_id

        if is_generation_in_progress(session_id):
            emit('error', {'message': 'Generation already in progress', 'session_id': session_id}, room=session_id)
            return

        # Check for last input audio
        last_input_base64 = retrieve_audio_data(session_id, 'last_input_audio')
        if last_input_base64 is None:
            emit('error', {'message': 'No last input audio available for retry', 'session_id': session_id}, room=session_id)
            return

        # Extract parameters with default values
        top_k = int(request_data.top_k) if request_data.top_k is not None else 250
        temperature = float(request_data.temperature) if request_data.temperature is not None else 1.0
        cfg_coef = float(request_data.cfg_coef) if request_data.cfg_coef is not None else 3.0
        description = request_data.description

        # Get model parameters from session if not provided
        model_name = request_data.model_name or sessions.find_one({'_id': session_id}).get('model_name')
        prompt_duration = request_data.prompt_duration or sessions.find_one({'_id': session_id}).get('prompt_duration')

        # Update session data in MongoDB
        mongo_data = {
            'model_name': model_name,
            'prompt_duration': prompt_duration,
            'parameters': {
                'top_k': top_k,
                'temperature': temperature,
                'cfg_coef': cfg_coef,
            },
            'description': description,
            'updated_at': datetime.now(timezone.utc)
        }
        
        sessions.update_one(
            {'_id': session_id},
            {'$set': mongo_data},
            upsert=True
        )

        # Prepare task data for queue
        task_data = {
            'model_name': str(model_name),
            'prompt_duration': int(prompt_duration),
            'top_k': int(top_k),
            'temperature': float(temperature),
            'cfg_coef': float(cfg_coef),
            'description': description if description else None
        }
        
        # Remove None values
        task_data = {k: v for k, v in task_data.items() if v is not None}
        
        # Queue the task
        queue_response = queue_task(session_id, 'retry_music', task_data)
        if not queue_response:
            emit('error', {'message': 'Failed to queue task', 'session_id': session_id})
            return

        # Join room for this session
        join_room(session_id)

        # Emit queue status to client
        # Emit queue status to client with time estimates
        # queue_status = get_queue_status_message(queue_response)
        # queue_status['session_id'] = session_id  # Add session_id to the response
        # emit('queue_status', queue_status)

        # Emit retry music received confirmation
        emit('retry_music_received', {'session_id': session_id})
        print(f"Emitted retry_music_received for session {session_id}")

    except ValidationError as e:
        emit('error', {'message': str(e), 'session_id': data.get('session_id') if isinstance(data, dict) else None})
    finally:
        thread_manager.cleanup_threads()

def handle_task_ready_retry(session_id):
    try:
        session_data = sessions.find_one({'_id': session_id})
        print(f"Retrieved session data for retry music {session_id}: {session_data}")
        
        if not session_data:
            raise ValueError(f"No session data found for {session_id}")

        # Get the last input audio for retry
        input_data_base64 = retrieve_audio_data(session_id, 'last_input_audio')
        if not input_data_base64:
            raise ValueError(f"No last input audio found for {session_id}")

        # Verify required fields with types
        model_name = str(session_data['model_name'])
        prompt_duration = int(session_data['prompt_duration'])
        parameters = session_data.get('parameters', {})
        
        # Mark as processing
        set_generation_in_progress(session_id, True)

         # Notify Go service that task is now processing
        requests.post(
            'http://gpu-queue:8085/task/status',
            json={
                'session_id': session_id,
                'status': 'processing'
            }
        )

        def retry_music_processing_thread():
            try:
                with track_processes():
                    def progress_callback(current, total):
                        progress_percent = (current / total) * 100
                        smart_emit('progress_update',
                             {'progress': int(progress_percent), 'session_id': session_id}, 
                             room=session_id)

                    # Use verified parameters
                    result_base64 = continue_music(
                        input_data_base64,
                        model_name,
                        progress_callback,
                        prompt_duration=prompt_duration,
                        top_k=parameters.get('top_k', 250),
                        temperature=parameters.get('temperature', 1.0),
                        cfg_coef=parameters.get('cfg_coef', 3.0),
                        description=session_data.get('description')
                    )
                    
                    # Store the same input audio again for potential future retries
                    store_audio_data(session_id, input_data_base64, 'last_input_audio')
                    store_audio_data(session_id, result_base64, 'last_processed_audio')
                    socketio.emit('music_retried',
                         {'audio_data': result_base64, 'session_id': session_id}, 
                         room=session_id)

                    # Notify Go service that task is complete
                    requests.post(
                        'http://gpu-queue:8085/task/status',
                        json={
                            'session_id': session_id,
                            'status': 'completed'
                        }
                    )

            except Exception as e:
                print(f"Error during retry music processing thread for session {session_id}: {e}")
                socketio.emit('error', {'message': str(e), 'session_id': session_id}, room=session_id)
                # Notify Go service of failure
                requests.post(
                    'http://gpu-queue:8085/task/status',
                    json={
                        'session_id': session_id,
                        'status': 'failed'
                    }
                )
            finally:
                set_generation_in_progress(session_id, False)
                clean_gpu_memory()
                thread_manager.cleanup_threads()

        enhanced_spawn(retry_music_processing_thread)

    except Exception as e:
        print(f"Error in handle_task_ready_retry for session {session_id}: {e}")
        socketio.emit('error', {
            'message': str(e),
            'session_id': session_id
        }, room=session_id)
        set_generation_in_progress(session_id, False)

@socketio.on('update_cropped_audio')
def handle_update_cropped_audio(data):
    try:
        data = parse_client_data(data)
        
        session_id = data.get('session_id')
        audio_data_base64 = data.get('audio_data')

        if not audio_data_base64:
            raise ValueError("Missing audio_data")

        # NEW: If no session exists, create one automatically
        if not session_id:
            session_id = generate_session_id()
            
            # Create minimal session with sensible defaults
            mongo_data = {
                '_id': session_id,
                'model_name': 'thepatch/vanya_ai_dnb_0.1',  # Default model
                'prompt_duration': 6,  # Default duration
                'parameters': {
                    'top_k': 250,
                    'temperature': 1.0,
                    'cfg_coef': 3.0,
                },
                'description': None,
                'created_at': datetime.now(timezone.utc),
                'created_from': 'jerry_crop'  # Flag to indicate origin
            }
            
            sessions.update_one(
                {'_id': session_id},
                {'$set': mongo_data},
                upsert=True
            )
            
            join_room(session_id)
            print(f"Auto-created session {session_id} from Jerry crop")

        # Store cropped audio as last_processed_audio (ready for continue)
        store_audio_data(session_id, audio_data_base64, 'last_processed_audio')
        
        emit('update_cropped_audio_complete', {
            'message': 'Cropped audio updated - ready to continue',
            'session_id': session_id,
            'auto_created': not data.get('session_id')  # Tell frontend if session was auto-created
        }, room=session_id)

        print(f"Cropped audio updated for session {session_id}")

    except Exception as e:
        session_id = data.get('session_id') if isinstance(data, dict) else 'unknown'
        print(f"Error in update_cropped_audio for session {session_id}: {e}")
        emit('error', {'message': str(e), 'session_id': session_id})

@socketio.on('restore_processed_audio')
def handle_restore_processed_audio(data):
    try:
        # Parse client data - any parsing errors will be caught by the outer try/except
        data = parse_client_data(data)

        # Get the audio data size
        audio_data_base64 = data.get('audio_data')
        if not audio_data_base64:
            raise ValueError("Missing audio data")

        # Check file size before processing
        data_size_bytes = len(audio_data_base64.encode('utf-8'))
        max_size_bytes = 64 * 1024 * 1024  # 64 MB

        if data_size_bytes > max_size_bytes:
            emit('error', {
                'message': 'Audio data is too large to restore',
                'code': 'DATA_TOO_LARGE'
            })
            return

        # Create a new session for the restored audio
        session_id = generate_session_id()
        join_room(session_id)

        model_name = data.get('model_name', '').replace("\\", "").strip()
        prompt_duration = data.get('prompt_duration')

        if not all([model_name, prompt_duration]):
            raise ValueError("Missing required parameters")

        # Store the audio data in the new session
        store_audio_data(session_id, audio_data_base64, 'last_processed_audio')

        # Store session settings
        sessions.update_one(
            {'_id': session_id},
            {
                '$set': {
                    'model_name': model_name,
                    'prompt_duration': prompt_duration,
                    'restored': True  # Flag to indicate this is a restored session
                }
            },
            upsert=True
        )

        emit('restore_complete', {
            'message': 'Audio restored successfully',
            'session_id': session_id,
            'data_size': data_size_bytes
        }, room=session_id)

    except Exception as e:
        print(f"Error in restore_processed_audio: {e}")
        emit('error', {'message': str(e)})

@socketio.on('begin_restore_audio')
def handle_begin_restore(data):
    try:
        session_id = generate_session_id()
        join_room(session_id)

        redis_client.set(f"{session_id}_restore_chunks", "")

        emit('ready_for_chunks', {
            'session_id': session_id,
            'chunk_size': 8 * 1024 * 1024  # 8MB chunks
        })

    except Exception as e:
        emit('error', {'message': str(e)})

@socketio.on('audio_chunk')
def handle_audio_chunk(data):
    try:
        # Handle string input just like restore_processed_audio
        # Parse client data - any parsing errors will be caught by the outer try/except
        data = parse_client_data(data)

        session_id = data.get('session_id')
        chunk = data.get('chunk')
        chunk_index = data.get('chunk_index')
        total_chunks = data.get('total_chunks')
        is_last = data.get('is_last', False)

        if not all([session_id, chunk, isinstance(chunk_index, int), isinstance(total_chunks, int)]):
            raise ValueError("Missing required chunk data")

        # Store chunk with a longer expiration time
        chunk_key = f"{session_id}_chunk_{chunk_index}"
        redis_client.setex(chunk_key, 3600, chunk)  # 1 hour expiration

        # Track received chunks in a Redis set
        received_chunks_key = f"{session_id}_received_chunks_set"
        redis_client.sadd(received_chunks_key, chunk_index)
        redis_client.expire(received_chunks_key, 3600)  # 1 hour expiration

        # Get count of received chunks
        received_count = redis_client.scard(received_chunks_key)

        # Acknowledge receipt
        emit('chunk_received', {
            'session_id': session_id,
            'chunk_index': chunk_index,
            'total_chunks': total_chunks,
            'received_chunks': received_count
        })

        print(f"Stored chunk {chunk_index} of {total_chunks} for session {session_id}")
        print(f"Received chunks count: {received_count}")

        # If this is the last chunk or we have all chunks, process them
        if is_last or received_count == total_chunks:
            # Verify we have all chunks
            all_chunks_present = True
            missing_chunks = []

            for i in range(total_chunks):
                chunk_exists = redis_client.exists(f"{session_id}_chunk_{i}")
                if not chunk_exists:
                    all_chunks_present = False
                    missing_chunks.append(i)
                    print(f"Missing chunk {i} for session {session_id}")

            if all_chunks_present:
                # Combine all chunks in order
                complete_audio = []
                for i in range(total_chunks):
                    chunk_key = f"{session_id}_chunk_{i}"
                    chunk_data = redis_client.get(chunk_key)
                    if chunk_data:
                        complete_audio.append(chunk_data.decode('utf-8'))
                        redis_client.delete(chunk_key)

                # Clean up
                redis_client.delete(received_chunks_key)

                # Store the complete audio
                complete_audio_data = ''.join(complete_audio)
                store_audio_data(session_id, complete_audio_data, 'last_processed_audio')

                # Store session settings
                if data.get('model_name') and data.get('prompt_duration'):
                    sessions.update_one(
                        {'_id': session_id},
                        {
                            '$set': {
                                'model_name': data.get('model_name'),
                                'prompt_duration': data.get('prompt_duration'),
                                'restored': True
                            }
                        },
                        upsert=True
                    )

                emit('restore_complete', {
                    'message': 'Audio restored successfully',
                    'session_id': session_id
                }, room=session_id)
            else:
                # Request missing chunks
                emit('chunks_missing', {
                    'session_id': session_id,
                    'missing_chunks': missing_chunks
                })

    except Exception as e:
        print(f"Error in handle_audio_chunk: {e}")
        emit('error', {'message': str(e)})

# Add this utility function for file management TRANSFORM ENDPOINT
def write_audio_to_temp_file(audio_base64, session_id):
    """Write base64 audio data to a temporary file and return the path."""
    try:
        # Create unique filename
        filename = f"input_{session_id}_{uuid.uuid4().hex[:8]}.wav"
        file_path = f"/tmp/audio_transfer/{filename}"
        
        # Ensure directory exists
        os.makedirs("/tmp/audio_transfer", exist_ok=True)
        
        # Decode and write binary data
        audio_data = base64.b64decode(audio_base64)
        with open(file_path, 'wb') as f:
            f.write(audio_data)
            
        return file_path
    except Exception as e:
        print(f"Error writing audio to temp file: {e}")
        return None

def cleanup_temp_file(file_path):
    """Safely remove a temporary file."""
    try:
        if file_path and os.path.exists(file_path):
            os.remove(file_path)
            print(f"Cleaned up temp file: {file_path}")
    except Exception as e:
        print(f"Error cleaning up temp file {file_path}: {e}")

@socketio.on('transform_audio_request')
def handle_transform_audio(data):
    try:
        clean_gpu_memory()
        thread_manager.cleanup_threads()
        data = parse_client_data(data)

        request_data = TransformRequest(**data)
        session_id = request_data.session_id or generate_session_id()

        # Check if a transform is already in progress for this session
        if is_transform_in_progress(session_id):
            emit('error', {'message': 'Transform already in progress', 'session_id': session_id}, room=session_id)
            return

        join_room(session_id)

        # Always emit this acknowledgment, regardless of whether it's a new or existing session
        emit('transform_audio_received', {'session_id': session_id})
        print(f"Emitted transform_audio_received for session {session_id}")

        # Get audio data
        if request_data.audio_data:
            input_data_base64 = request_data.audio_data
        else:
            input_data_base64 = retrieve_audio_data(session_id, 'last_processed_audio')
            if input_data_base64 is None:
                emit('error', {'message': 'No audio data available for transformation', 'session_id': session_id}, room=session_id)
                return

        # Store the audio data for later use
        store_audio_data(session_id, input_data_base64, 'transform_input_audio')

        # Prepare transform task data
        task_data = {
            'variation': request_data.variation,
            'session_id': session_id
        }
        
        # Add optional parameters if they exist
        if hasattr(request_data, 'flowstep') and request_data.flowstep is not None:
            task_data['flowstep'] = request_data.flowstep
        
        if hasattr(request_data, 'solver') and request_data.solver is not None:
            task_data['solver'] = request_data.solver.lower()
        
        # Add custom prompt if provided
        if hasattr(request_data, 'custom_prompt') and request_data.custom_prompt is not None:
            task_data['custom_prompt'] = request_data.custom_prompt

        # Store task data in Redis
        store_transform_task_data(session_id, task_data)

        # Queue the task with the Go service
        queue_response = queue_transform_task(session_id, 'transform_audio', task_data)
        if not queue_response:
            emit('error', {'message': 'Failed to queue transform task', 'session_id': session_id}, room=session_id)
            return

        # Emit queue status to client
        # queue_status = get_queue_status_message(queue_response)
        # queue_status['session_id'] = session_id
        # smart_emit('queue_status', queue_status, room=session_id)

    except ValidationError as e:
        emit('error', {'message': str(e), 'session_id': data.get('session_id') if isinstance(data, dict) else None})
    finally:
        thread_manager.cleanup_threads()


# Modified handle_task_ready_transform function
def handle_task_ready_transform(session_id):
    try:
        print(f"Starting transform task for session {session_id}")
        
        # Get the input audio data we stored earlier
        input_data_base64 = retrieve_audio_data(session_id, 'transform_input_audio')
        if not input_data_base64:
            raise ValueError(f"No input audio found for transform session {session_id}")

        # Write audio to temporary file instead of sending base64
        temp_input_file = write_audio_to_temp_file(input_data_base64, session_id)
        if not temp_input_file:
            raise ValueError("Failed to write audio to temporary file")

        # Get the transform parameters from Redis
        task_data_json = redis_client.get(f"transform_task:{session_id}:data")
        if not task_data_json:
            cleanup_temp_file(temp_input_file)
            raise ValueError(f"No task data found for transform session {session_id}")

        task_data = json.loads(task_data_json.decode('utf-8'))
        
        # Set transform in progress in Redis
        set_transform_in_progress(session_id, True)
        
        # Notify Go service that task is now processing
        requests.post(
            'http://gpu-queue:8085/transform/task/status',
            json={
                'session_id': session_id,
                'status': 'processing'
            }
        )

        def transform_audio_thread():
            flask_socket = None
            temp_output_file = None
            try:
                with track_processes():
                    
                    def context_emit(event, data):
                        smart_emit(event, data, room=session_id)

                    print(f"[DEBUG] Starting transform_audio_thread for session {session_id}")
                    flask_socket = Client(reconnection=True, reconnection_attempts=5, reconnection_delay=1)

                    @flask_socket.on('progress')
                    def handle_progress(data):
                        if data.get('session_id') == session_id:
                            context_emit('progress_update', {
                                'progress': data.get('progress', 0),
                                'session_id': session_id
                            })

                    print(f"[DEBUG] Connecting to MelodyFlow WebSocket for session {session_id}")
                    flask_socket.connect('http://melodyflow:8002', wait_timeout=30)
                    print(f"[DEBUG] Connected to MelodyFlow WebSocket")

                    # Prepare request payload with file path instead of base64
                    request_payload = {
                        'audio_file_path': temp_input_file,  # NEW: Send file path instead
                        'variation': task_data.get('variation'),
                        'session_id': session_id
                    }

                    # Add optional parameters if they exist
                    if 'flowstep' in task_data and task_data['flowstep'] is not None:
                        request_payload['flowstep'] = task_data['flowstep']
                    
                    if 'solver' in task_data and task_data['solver'] is not None:
                        request_payload['solver'] = task_data['solver']

                    # Add custom prompt if provided
                    if 'custom_prompt' in task_data and task_data['custom_prompt'] is not None:
                        request_payload['custom_prompt'] = task_data['custom_prompt']

                    print(f"[DEBUG] Sending transform request to MelodyFlow for session {session_id}")
                    response = requests.post(
                        'http://melodyflow:8002/transform',
                        json=request_payload
                    )
                    print(f"[DEBUG] Received transform response: status={response.status_code}")
                    
                    if response.status_code == 200:
                        # Immediate result
                        result = response.json()
                        
                        # Check if response contains file path or base64
                        if 'audio_file_path' in result:
                            # Read result from file
                            temp_output_file = result['audio_file_path']
                            with open(temp_output_file, 'rb') as f:
                                audio_data = f.read()
                            result_base64 = base64.b64encode(audio_data).decode('utf-8')
                        else:
                            # Backward compatibility - still accept base64 response
                            result_base64 = result['audio']

                        # Store audio data (keeping base64 for Redis storage)
                        store_audio_data(session_id, input_data_base64, 'last_input_audio')
                        store_audio_data(session_id, result_base64, 'last_processed_audio')

                        # Check size before sending
                        result_size_bytes = len(result_base64.encode('utf-8'))
                        max_size_bytes = 128 * 1024 * 1024

                        if result_size_bytes > max_size_bytes:
                            smart_emit('error', {
                                'message': 'Transformed audio data is too large to send.',
                                'session_id': session_id,
                                'code': 'DATA_TOO_LARGE'
                            }, room=session_id)
                        else:
                            smart_emit('audio_transformed', {
                                'audio_data': result_base64,
                                'session_id': session_id,
                                'variation': task_data.get('variation')
                            }, room=session_id, force=True)
                            
                    elif response.status_code == 202:
                        # Queued processing - let MelodyFlow handle it
                        response_data = response.json()
                        job_id = response_data.get('job_id')
                        
                        if not job_id:
                            raise ValueError("No job_id in MelodyFlow response")
                            
                        print(f"[DEBUG] MelodyFlow job ID: {job_id} for session {session_id}")
                        
                    else:
                        # Error occurred
                        error_message = response.json().get('error', f'Unknown error during transformation (code: {response.status_code})')
                        raise ValueError(error_message)

                    # Notify Go service that task is complete
                    requests.post(
                        'http://gpu-queue:8085/transform/task/status',
                        json={
                            'session_id': session_id,
                            'status': 'completed'
                        }
                    )

            except Exception as e:
                print(f"[DEBUG] Exception in transform_audio_thread for session {session_id}: {e}")
                import traceback
                traceback.print_exc()
                smart_emit('error', {'message': str(e), 'session_id': session_id}, room=session_id)
                
                # Notify Go service of failure
                try:
                    requests.post(
                        'http://gpu-queue:8085/transform/task/status',
                        json={
                            'session_id': session_id,
                            'status': 'failed'
                        }
                    )
                except Exception as status_err:
                    print(f"Error updating transform task status to failed: {status_err}")
            finally:
                print(f"[DEBUG] Cleaning up transform_audio_thread for session {session_id}")
                if flask_socket and flask_socket.connected:
                    flask_socket.disconnect()
                
                # Clean up temporary files
                cleanup_temp_file(temp_input_file)
                if temp_output_file:
                    cleanup_temp_file(temp_output_file)
                    
                set_transform_in_progress(session_id, False)
                clean_gpu_memory()
                thread_manager.cleanup_threads()

        enhanced_spawn(transform_audio_thread)

    except Exception as e:
        print(f"Error in handle_task_ready_transform for session {session_id}: {e}")
        cleanup_temp_file(temp_input_file if 'temp_input_file' in locals() else None)
        smart_emit('error', {
            'message': str(e),
            'session_id': session_id
        }, room=session_id)
        
        # Update task status to failed in queue
        try:
            requests.post(
                'http://gpu-queue:8085/transform/task/status',
                json={
                    'session_id': session_id,
                    'status': 'failed'
                }
            )
        except Exception as status_err:
            print(f"Error updating transform task status to failed: {status_err}")
            
        set_transform_in_progress(session_id, False)

@socketio.on('undo_transform_request')
def handle_undo_transform(data):
    try:
        request_data = SessionRequest(**data)
        session_id = request_data.session_id

        if not session_id:
            emit('error', {'message': 'No session ID provided for undo'})
            return

        # Retrieve the last input audio that was transformed
        last_input_base64 = retrieve_audio_data(session_id, 'last_input_audio')
        if last_input_base64 is None:
            emit('error', {
                'message': 'No previous audio found to undo to',
                'session_id': session_id
            }, room=session_id)
            return

        # Update the last_processed_audio to be the previous input
        store_audio_data(session_id, last_input_base64, 'last_processed_audio')

        # Send the previous audio back to the client
        emit('transform_undone', {
            'audio_data': last_input_base64,
            'session_id': session_id
        }, room=session_id)

    except Exception as e:
        print(f"Error during undo_transform for session {session_id}: {e}")
        emit('error', {'message': str(e), 'session_id': session_id})

# Robust Health Check Route
@app.route('/health', methods=['GET'])
def health_check():
    health_status = {"status": "live"}

    # Check MongoDB
    try:
        client.admin.command('ping')
        health_status['mongodb'] = 'live'
    except Exception as e:
        health_status['mongodb'] = f'down: {str(e)}'
        health_status['status'] = 'degraded'

    # Check Redis
    try:
        redis_client.ping()
        health_status['redis'] = 'live'
    except Exception as e:
        health_status['redis'] = f'down: {str(e)}'
        health_status['status'] = 'degraded'

    # Check PyTorch (Optional, if it's critical)
    try:
        if torch.cuda.is_available():
            health_status['pytorch'] = 'live'
        else:
            health_status['pytorch'] = 'no GPU detected'
    except Exception as e:
        health_status['pytorch'] = f'down: {str(e)}'
        health_status['status'] = 'degraded'

    return jsonify(health_status), 200 if health_status['status'] == 'live' else 500

@app.route('/api/latest_generated_audio/<session_id>', methods=['GET'])
def get_latest_generated_audio(session_id):
    """
    Retrieve the most recently generated audio for a session if generation completed.
    This endpoint is specifically for recovering audio that was generated while the app was minimized.
    """
    try:
        # Check if generation is still in progress
        if is_generation_in_progress(session_id):
            return jsonify({
                'status': 'in_progress',
                'message': 'Audio generation still in progress'
            }), 202

        # Retrieve the session data
        session_data = sessions.find_one({'_id': session_id})
        if not session_data:
            return jsonify({
                'status': 'not_found',
                'message': 'Session not found'
            }), 404

        # Get the most recently processed audio
        audio_data = retrieve_audio_data(session_id, 'last_processed_audio')
        if not audio_data:
            return jsonify({
                'status': 'no_audio',
                'message': 'No processed audio found for this session'
            }), 404

        # Return the audio data along with session parameters needed for restoration
        return jsonify({
            'status': 'success',
            'audio_data': audio_data,
            'model_name': session_data.get('model_name'),
            'prompt_duration': session_data.get('prompt_duration'),
            'session_id': session_id
        }), 200

    except Exception as e:
        print(f"Error retrieving latest generated audio for session {session_id}: {e}")
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500

# Add a cleanup mechanism to remove old sessions
@app.route('/api/cleanup_old_sessions', methods=['POST'])
def cleanup_old_sessions():
    cleanup_stats = {
        "mongodb_sessions": 0,
        "gridfs_files": 0,
        "redis_keys": 0,
        "errors": []
    }
    
    try:
        threshold_time = datetime.now(timezone.utc) - timedelta(hours=24)
        
        # 1. GridFS cleanup - one at a time
        try:
            cursor = fs.find({'uploadDate': {'$lt': threshold_time}})
            for file_doc in cursor:
                try:
                    fs.delete(file_doc._id)
                    cleanup_stats["gridfs_files"] += 1
                    gevent.sleep(0.2)  # Longer delay between operations
                except Exception as e:
                    cleanup_stats["errors"].append(f"GridFS file deletion error: {str(e)}")
        except Exception as e:
            cleanup_stats["errors"].append(f"GridFS cursor error: {str(e)}")

        # 2. MongoDB sessions cleanup - do this second since it's lighter
        try:
            result = sessions.delete_many({
                'created_at': {'$lt': threshold_time}
            })
            cleanup_stats["mongodb_sessions"] = result.deleted_count
        except Exception as e:
            cleanup_stats["errors"].append(f"Session cleanup error: {str(e)}")
        
        # 3. Redis cleanup
        redis_count = 0
        try:
            for key in redis_client.scan_iter("*_generation_in_progress"):
                try:
                    session_id = key.decode('utf-8').split('_')[0]
                    if not sessions.find_one({'_id': session_id}):
                        redis_client.delete(key)
                        redis_client.delete(f"{session_id}_progress")
                        redis_client.delete(f"{session_id}_received_chunks_set")
                        # Clean up chunks
                        for chunk_key in redis_client.scan_iter(f"{session_id}_chunk_*"):
                            redis_client.delete(chunk_key)
                        redis_count += 1
                        if redis_count % 10 == 0:
                            gevent.sleep(0.1)
                except Exception as e:
                    cleanup_stats["errors"].append(f"Redis key cleanup error: {str(e)}")
        except Exception as e:
            cleanup_stats["errors"].append(f"Redis cleanup error: {str(e)}")
                
        cleanup_stats["redis_keys"] = redis_count
        
        # Add memory stats
        process = psutil.Process()
        cleanup_stats["memory_before_gc"] = round(process.memory_info().rss / 1024 / 1024, 2)  # MB
        gc.collect()
        cleanup_stats["memory_after_gc"] = round(process.memory_info().rss / 1024 / 1024, 2)  # MB
        
        return jsonify({
            "status": "success" if not cleanup_stats["errors"] else "partial_success",
            "cleanup_stats": cleanup_stats,
            "message": "Cleanup completed successfully" if not cleanup_stats["errors"] else "Cleanup completed with some errors"
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e),
            "cleanup_stats": cleanup_stats
        }), 500
    
# MARK: http version
    
@app.route('/api/juce/process_audio', methods=['POST'])
def juce_process_audio():
    """Start audio processing - equivalent to 'process_audio_request' event"""
    try:
        print(f"[DEBUG] HTTP endpoint called")
        
        # Step 1: Get raw data
        raw_data = request.json
        print(f"[DEBUG] Raw received data keys: {list(raw_data.keys()) if raw_data else 'None'}")
        
        # Step 2: Clean the data FIRST (like WebSocket handler does)
        try:
            cleaned_data = parse_client_data(raw_data)
            print(f"[DEBUG] After cleaning data keys: {list(cleaned_data.keys()) if cleaned_data else 'None'}")
        except ValueError as e:
            print(f"[ERROR] Data cleaning failed: {str(e)}")
            return jsonify({'success': False, 'error': str(e)}), 400
        
        # Step 3: Validate CLEANED data
        request_data = AudioRequest(**cleaned_data)  # ✅ Use cleaned data
        print(f"[DEBUG] AudioRequest validation passed")
        print(f"[DEBUG] Model: {request_data.model_name}")
        print(f"[DEBUG] Duration: {request_data.prompt_duration} (type: {type(request_data.prompt_duration)})")
        
        # Check audio data size
        audio_data = cleaned_data.get('audio_data', '')
        print(f"[DEBUG] Audio data length: {len(audio_data)}")
        
        session_id = generate_session_id()
        print(f"[DEBUG] Generated session_id: {session_id}")
        
        # Step 4: Use CLEANED data for session creation
        mongo_data = {
            '_id': session_id,
            'model_name': str(request_data.model_name),
            'prompt_duration': int(request_data.prompt_duration),
            'parameters': {
                'top_k': int(request_data.top_k) if request_data.top_k is not None else 250,
                'temperature': float(request_data.temperature) if request_data.temperature is not None else 1.0,
                'cfg_coef': float(request_data.cfg_coef) if request_data.cfg_coef is not None else 3.0,
            },
            'description': request_data.description,
            'created_at': datetime.now(timezone.utc)
        }
        print(f"[DEBUG] MongoDB data prepared: {mongo_data}")
        
        result = sessions.update_one({'_id': session_id}, {'$set': mongo_data}, upsert=True)
        print(f"[DEBUG] MongoDB insert result: matched={result.matched_count}, modified={result.modified_count}")
        
        # Step 5: Store audio data from CLEANED data
        print(f"[DEBUG] Storing audio data...")
        store_audio_data(session_id, request_data.audio_data, 'initial_audio')
        print(f"[DEBUG] Audio data stored successfully")
        
        # Step 6: Queue task with CLEANED data
        print(f"[DEBUG] Queueing task...")
        queue_response = queue_task(session_id, 'process_audio', cleaned_data)  # ✅ Use cleaned data
        print(f"[DEBUG] Queue response: {queue_response}")
        
        if not queue_response:
            print(f"[ERROR] Failed to queue task")
            return jsonify({'success': False, 'error': 'Failed to queue task'}), 500
        
        print(f"[DEBUG] HTTP endpoint completed successfully")
        return jsonify({
            'success': True,
            'session_id': session_id,
            'message': 'Audio processing queued',
            'queue_status': get_queue_status_message(queue_response)
        })
        
    except ValidationError as e:
        print(f"[ERROR] Validation error: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 400
    except Exception as e:
        print(f"[ERROR] Unexpected error in HTTP endpoint: {str(e)}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/juce/continue_music', methods=['POST'])
def juce_continue_music():
    """Continue music from existing session - equivalent to 'continue_music_request' event"""
    try:
        data = request.json
        request_data = ContinueMusicRequest(**data)
        session_id = request_data.session_id
        
        # Auto-create session if audio_data provided (same logic as Socket.IO)
        if not session_id and request_data.audio_data:
            session_id = generate_session_id()
            mongo_data = {
                '_id': session_id,
                'model_name': request_data.model_name or 'thepatch/vanya_ai_dnb_0.1',
                'prompt_duration': request_data.prompt_duration or 6,
                'parameters': {
                    'top_k': int(request_data.top_k) if request_data.top_k else 250,
                    'temperature': float(request_data.temperature) if request_data.temperature else 1.0,
                    'cfg_coef': float(request_data.cfg_coef) if request_data.cfg_coef else 3.0,
                },
                'description': request_data.description,
                'created_at': datetime.now(timezone.utc),
                'created_from': 'juce_continue'
            }
            sessions.update_one({'_id': session_id}, {'$set': mongo_data}, upsert=True)
            store_audio_data(session_id, request_data.audio_data, 'last_processed_audio')
        
        elif not session_id:
            return jsonify({'success': False, 'error': 'Session ID required'}), 400
        
        if is_generation_in_progress(session_id):
            return jsonify({'success': False, 'error': 'Generation already in progress'}), 400
        
        # Same parameter handling as Socket.IO
        session_data = sessions.find_one({'_id': session_id})
        if not session_data:
            return jsonify({'success': False, 'error': 'Session not found'}), 404
        
        model_name = request_data.model_name or session_data.get('model_name')
        prompt_duration = request_data.prompt_duration or session_data.get('prompt_duration')
        
        # Store audio data if provided
        if request_data.audio_data:
            store_audio_data(session_id, request_data.audio_data, 'last_processed_audio')
        
        task_data = {
            'model_name': model_name,
            'prompt_duration': prompt_duration,
            'top_k': int(request_data.top_k) if request_data.top_k is not None else 250,
            'temperature': float(request_data.temperature) if request_data.temperature is not None else 1.0,
            'cfg_coef': float(request_data.cfg_coef) if request_data.cfg_coef is not None else 3.0,
            'description': request_data.description
        }
        
        queue_response = queue_task(session_id, 'continue_music', task_data)
        if not queue_response:
            return jsonify({'success': False, 'error': 'Failed to queue task'}), 500
        
        return jsonify({
            'success': True,
            'session_id': session_id,
            'message': 'Music continuation queued',
            'queue_status': get_queue_status_message(queue_response)
        })
        
    except ValidationError as e:
        return jsonify({'success': False, 'error': str(e)}), 400
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/juce/transform_audio', methods=['POST'])
def juce_transform_audio():
    """Transform audio - equivalent to 'transform_audio_request' event"""
    try:
        data = request.json
        request_data = TransformRequest(**data)
        session_id = request_data.session_id or generate_session_id()
        
        if is_transform_in_progress(session_id):
            return jsonify({'success': False, 'error': 'Transform already in progress'}), 400
        
        # Get audio data (same logic as Socket.IO)
        if request_data.audio_data:
            input_data_base64 = request_data.audio_data
        else:
            input_data_base64 = retrieve_audio_data(session_id, 'last_processed_audio')
            if input_data_base64 is None:
                return jsonify({'success': False, 'error': 'No audio data available'}), 400
        
        # Store transform input (for undo functionality)
        store_audio_data(session_id, input_data_base64, 'transform_input_audio')
        
        task_data = {
            'variation': request_data.variation,
            'session_id': session_id
        }
        
        if hasattr(request_data, 'flowstep') and request_data.flowstep is not None:
            task_data['flowstep'] = request_data.flowstep
        if hasattr(request_data, 'solver') and request_data.solver is not None:
            task_data['solver'] = request_data.solver.lower()
        if hasattr(request_data, 'custom_prompt') and request_data.custom_prompt is not None:
            task_data['custom_prompt'] = request_data.custom_prompt
        
        store_transform_task_data(session_id, task_data)
        
        queue_response = queue_transform_task(session_id, 'transform_audio', task_data)
        if not queue_response:
            return jsonify({'success': False, 'error': 'Failed to queue transform task'}), 500
        
        return jsonify({
            'success': True,
            'session_id': session_id,
            'message': 'Audio transform queued',
            'queue_status': get_queue_status_message(queue_response)
        })
        
    except ValidationError as e:
        return jsonify({'success': False, 'error': str(e)}), 400
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/juce/undo_transform', methods=['POST'])
def juce_undo_transform():
    """Undo transform - equivalent to 'undo_transform_request' event"""
    try:
        data = request.json
        session_id = data.get('session_id')
        
        if not session_id:
            return jsonify({'success': False, 'error': 'Session ID required'}), 400
        
        # Retrieve the last input audio that was transformed
        last_input_base64 = retrieve_audio_data(session_id, 'last_input_audio')
        if last_input_base64 is None:
            return jsonify({'success': False, 'error': 'No previous audio found to undo to'}), 400
        
        # Update the last_processed_audio to be the previous input
        store_audio_data(session_id, last_input_base64, 'last_processed_audio')
        
        return jsonify({
            'success': True,
            'session_id': session_id,
            'message': 'Transform undone',
            'audio_data': last_input_base64
        })
        
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/juce/retry_music', methods=['POST'])
def juce_retry_music():
    """Retry music generation - equivalent to 'retry_music_request' event"""
    try:
        data = request.json
        request_data = SessionRequest(**data)
        session_id = request_data.session_id
        
        if not session_id:
            return jsonify({'success': False, 'error': 'Session ID required'}), 400
        
        if is_generation_in_progress(session_id):
            return jsonify({'success': False, 'error': 'Generation already in progress'}), 400
        
        # Check for last input audio
        last_input_base64 = retrieve_audio_data(session_id, 'last_input_audio')
        if last_input_base64 is None:
            return jsonify({'success': False, 'error': 'No last input audio available for retry'}), 400
        
        task_data = {
            'model_name': request_data.model_name,
            'prompt_duration': request_data.prompt_duration,
            'top_k': int(request_data.top_k) if request_data.top_k is not None else 250,
            'temperature': float(request_data.temperature) if request_data.temperature is not None else 1.0,
            'cfg_coef': float(request_data.cfg_coef) if request_data.cfg_coef is not None else 3.0,
            'description': request_data.description
        }
        
        queue_response = queue_task(session_id, 'retry_music', task_data)
        if not queue_response:
            return jsonify({'success': False, 'error': 'Failed to queue task'}), 500
        
        return jsonify({
            'success': True,
            'session_id': session_id,
            'message': 'Music retry queued',
            'queue_status': get_queue_status_message(queue_response)
        })
        
    except ValidationError as e:
        return jsonify({'success': False, 'error': str(e)}), 400
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/juce/poll_status/<session_id>')
def juce_poll_status(session_id):
    """Poll for session status, progress, and results"""
    try:
        # Check if any generation is in progress
        generation_in_progress = is_generation_in_progress(session_id)
        transform_in_progress = is_transform_in_progress(session_id)
        
        # Get session data
        session_data = sessions.find_one({'_id': session_id})
        if not session_data:
            return jsonify({'success': False, 'error': 'Session not found'}), 404
        
        # Get current progress and status (NEW)
        current_progress = get_session_progress(session_id)
        current_status = get_session_status(session_id)
        
        # Get queue status from Go service (EXISTING)
        queue_status = {}
        try:
            response = requests.get(f'http://gpu-queue:8085/tasks/{session_id}')
            if response.status_code == 200:
                queue_status = response.json()
        except:
            pass
        
        # Get completed audio if processing is done (EXISTING)
        audio_data = None
        if not generation_in_progress and not transform_in_progress:
            audio_data = retrieve_audio_data(session_id, 'last_processed_audio')
        
        return jsonify({
            'success': True,
            'session_id': session_id,
            'generation_in_progress': generation_in_progress,
            'transform_in_progress': transform_in_progress,
            'progress': current_progress,  # NEW: Current progress (0-100)
            'status': current_status['status'],  # NEW: current status (processing/completed/failed)
            'error': current_status.get('error'),  # NEW: Error message if failed
            'audio_data': audio_data,
            'queue_status': queue_status,
            'session_data': {
                'model_name': session_data.get('model_name'),
                'prompt_duration': session_data.get('prompt_duration'),
                'parameters': session_data.get('parameters', {}),
                'description': session_data.get('description')
            }
        })
        
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/juce/update_cropped_audio', methods=['POST'])
def juce_update_cropped_audio():
    """Update cropped audio - equivalent to 'update_cropped_audio' event"""
    try:
        data = request.json
        session_id = data.get('session_id')
        audio_data_base64 = data.get('audio_data')
        
        if not audio_data_base64:
            return jsonify({'success': False, 'error': 'Missing audio_data'}), 400
        
        # Auto-create session if needed (same logic as Socket.IO)
        if not session_id:
            session_id = generate_session_id()
            mongo_data = {
                '_id': session_id,
                'model_name': 'thepatch/vanya_ai_dnb_0.1',
                'prompt_duration': 6,
                'parameters': {
                    'top_k': 250,
                    'temperature': 1.0,
                    'cfg_coef': 3.0,
                },
                'description': None,
                'created_at': datetime.now(timezone.utc),
                'created_from': 'juce_crop'
            }
            sessions.update_one({'_id': session_id}, {'$set': mongo_data}, upsert=True)
        
        # Store cropped audio as last_processed_audio
        store_audio_data(session_id, audio_data_base64, 'last_processed_audio')
        
        return jsonify({
            'success': True,
            'session_id': session_id,
            'message': 'Cropped audio updated - ready to continue',
            'auto_created': not data.get('session_id')
        })
        
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

if __name__ == '__main__':
    socketio.run(app, debug=True)
