#!/usr/bin/env python3
"""
Test script to verify the nonce validation logic matches between ESP32 and Flask
"""

def test_nonce_validation():
    """Test the exact nonce validation logic that was implemented"""
    
    # Test case 1: First-time sync (last_received_nonce = 0)
    print("=== Test Case 1: First-time sync ===")
    last_received_nonce = 0
    current_nonce = 302  # What Flask sent
    nonce_window = 100
    max_first_sync = 1000
    
    # Original logic (would fail)
    original_valid = current_nonce > last_received_nonce and (current_nonce - last_received_nonce) <= nonce_window
    print(f"Original logic: nonce={current_nonce}, last={last_received_nonce}, window={nonce_window}")
    print(f"Original result: {original_valid}")
    
    # New logic (should pass)
    if last_received_nonce == 0:
        new_valid = current_nonce > 0 and current_nonce <= max_first_sync
        print(f"New first-sync logic: nonce={current_nonce}, max_first_sync={max_first_sync}")
        print(f"New result: {new_valid}")
    else:
        new_valid = current_nonce > last_received_nonce and (current_nonce - last_received_nonce) <= nonce_window
        print(f"New normal logic: nonce={current_nonce}, last={last_received_nonce}, window={nonce_window}")
        print(f"New result: {new_valid}")
    
    print()
    
    # Test case 2: Normal operation after sync
    print("=== Test Case 2: Normal operation after sync ===")
    last_received_nonce = 302  # Device accepted first nonce
    current_nonce = 350  # Next nonce from Flask
    
    if last_received_nonce == 0:
        new_valid = current_nonce > 0 and current_nonce <= max_first_sync
        print(f"New first-sync logic: nonce={current_nonce}, max_first_sync={max_first_sync}")
    else:
        new_valid = current_nonce > last_received_nonce and (current_nonce - last_received_nonce) <= nonce_window
        print(f"New normal logic: nonce={current_nonce}, last={last_received_nonce}, window={nonce_window}")
    
    print(f"New result: {new_valid}")
    print()
    
    # Test case 3: Edge case - nonce exactly at window limit
    print("=== Test Case 3: Edge case - nonce at window limit ===")
    last_received_nonce = 302
    current_nonce = 402  # Exactly 100 ahead
    
    new_valid = current_nonce > last_received_nonce and (current_nonce - last_received_nonce) <= nonce_window
    print(f"Edge case logic: nonce={current_nonce}, last={last_received_nonce}, window={nonce_window}")
    print(f"Difference: {current_nonce - last_received_nonce}")
    print(f"Result: {new_valid}")

if __name__ == "__main__":
    test_nonce_validation()