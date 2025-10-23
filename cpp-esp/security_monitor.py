#!/usr/bin/env python3
"""
Security Log Monitor for EcoWatt Demo
====================================

This script connects to the Flask server and displays real-time
security events for video demonstration purposes.

Usage:
    python security_monitor.py

Shows:
- HMAC verification successes/failures
- Replay attack detections
- Nonce validation events
- Authentication attempts
"""

import requests
import time
import json
from datetime import datetime

SERVER_URL = "http://10.50.126.183:8080"

def get_security_logs():
    """Fetch latest security logs from server."""
    try:
        response = requests.get(f"{SERVER_URL}/api/cloud/logs/security")
        if response.status_code == 200:
            return response.json().get('logs', [])
        return []
    except:
        return []

def format_security_event(event):
    """Format security event for display."""
    timestamp = event.get('timestamp', 'Unknown')
    device_id = event.get('device_id', 'Unknown')
    event_type = event.get('event_type', 'Unknown')
    details = event.get('details', 'No details')
    
    # Color coding for different event types
    if event_type == 'hmac_verified':
        icon = "✅"
        color = "green"
    elif 'attack' in event_type or 'failed' in event_type:
        icon = "🚨"
        color = "red"
    elif 'missing' in event_type:
        icon = "⚠️"
        color = "yellow"
    else:
        icon = "📋"
        color = "blue"
    
    return f"{icon} [{timestamp}] {device_id}: {event_type.upper()} - {details}"

def monitor_security_logs():
    """Monitor security logs in real-time."""
    print("🛡️  EcoWatt Security Monitor")
    print("===========================")
    print("Monitoring security events in real-time...")
    print("Press Ctrl+C to stop")
    print()
    
    seen_events = set()
    
    try:
        while True:
            logs = get_security_logs()
            
            # Show only new events
            for event in logs:
                event_id = f"{event.get('timestamp')}-{event.get('event_type')}-{event.get('device_id')}"
                if event_id not in seen_events:
                    print(format_security_event(event))
                    seen_events.add(event_id)
            
            time.sleep(2)  # Check every 2 seconds
            
    except KeyboardInterrupt:
        print("\\n👋 Security monitoring stopped.")
    except Exception as e:
        print(f"❌ Error: {e}")

def show_current_security_status():
    """Show current security configuration and status."""
    print("🛡️  Current Security Status")
    print("===========================")
    
    try:
        # Try to get server info
        response = requests.get(f"{SERVER_URL}/api/cloud/status")
        if response.status_code == 200:
            status = response.json()
            security_enabled = status.get('security_enabled', False)
            print(f"Security Enabled: {'✅ YES' if security_enabled else '❌ NO'}")
            print(f"Nonce Window: {status.get('nonce_window', 'Unknown')}")
            print(f"Active Devices: {len(status.get('device_nonces', {}))}")
        else:
            print("❌ Could not retrieve server status")
    except:
        print("⚠️  Server status unavailable")
    
    print()

if __name__ == "__main__":
    show_current_security_status()
    monitor_security_logs()