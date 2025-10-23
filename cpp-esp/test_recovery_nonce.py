#!/usr/bin/env python3
"""
Test script to verify the enhanced nonce recovery logic.
This simulates what happens when the device can't load nonce state.
"""

def simulate_recovery_nonce(current_nonce=1, uptime_ms=0):
    """Simulate the estimateRecoveryNonce() function"""
    
    recovery_base = current_nonce  # Start with constructor default (1)
    
    # Strategy 1: Use time-based estimation if available
    if uptime_ms > 60000:  # If we've been running for more than 1 minute
        # Estimate based on typical usage patterns - device usually makes requests every ~30 seconds
        # So after 1 minute of uptime, we might have made 2-3 requests
        estimated_requests = (uptime_ms // 30000) + 1  # Conservative estimate
        recovery_base = max(recovery_base, estimated_requests)
    
    # Strategy 2: Use a safe offset to avoid immediate conflicts
    # Add a buffer based on typical session length
    recovery_base += 50  # Safe buffer to avoid conflicts with recent server nonces
    
    # Strategy 3: Ensure we don't go backwards if we already have a reasonable value
    if current_nonce > 100:
        recovery_base = current_nonce + 10  # Continue from current with small buffer
    
    return recovery_base

def test_recovery_scenarios():
    print("=== Enhanced Nonce Recovery Logic Test ===\n")
    
    scenarios = [
        # (description, current_nonce, uptime_ms)
        ("Fresh boot, no uptime", 1, 0),
        ("Fresh boot, 2 minutes uptime", 1, 120000),
        ("Fresh boot, 5 minutes uptime", 1, 300000),
        ("After FOTA with cached nonce 201", 201, 60000),
        ("Long-running device with high nonce", 500, 180000),
    ]
    
    for desc, current, uptime in scenarios:
        recovery = simulate_recovery_nonce(current, uptime)
        print(f"Scenario: {desc}")
        print(f"  Current nonce: {current}")
        print(f"  Uptime: {uptime/1000:.1f} seconds")
        print(f"  Recovery nonce: {recovery}")
        print(f"  Buffer added: {recovery - current}")
        print()

def test_first_sync_compatibility():
    print("=== First-Time Sync Compatibility Test ===\n")
    
    def is_nonce_valid_enhanced(nonce, last_received, window=100, max_first_sync=1000):
        """Enhanced nonce validation with first-time sync support"""
        if last_received == 0:
            # First-time sync - allow reasonable nonces
            return nonce > 0 and nonce <= max_first_sync
        else:
            # Normal operation - enforce window
            return nonce > last_received and nonce <= (last_received + window)
    
    # Test recovery nonces against enhanced validation
    recovery_cases = [
        simulate_recovery_nonce(1, 0),      # ~51
        simulate_recovery_nonce(1, 120000), # ~54
        simulate_recovery_nonce(201, 60000), # ~251
    ]
    
    for recovery_nonce in recovery_cases:
        valid = is_nonce_valid_enhanced(recovery_nonce, 0, 100, 1000)
        print(f"Recovery nonce {recovery_nonce} with last_received=0: {'VALID' if valid else 'INVALID'}")
    
    print("\nAll recovery nonces should be VALID for first-time sync!")

if __name__ == "__main__":
    test_recovery_scenarios()
    print()
    test_first_sync_compatibility()