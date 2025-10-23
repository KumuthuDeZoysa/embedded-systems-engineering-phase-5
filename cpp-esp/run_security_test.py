# 🧪 Quick Security Test Script
# This temporarily modifies platformio.ini to upload test_security.cpp

import os
import shutil
from pathlib import Path

def backup_main_file():
    """Backup main.ino"""
    src_dir = Path("src")
    backup_dir = Path(".backup")
    backup_dir.mkdir(exist_ok=True)
    
    main_file = src_dir / "main.ino"
    backup_file = backup_dir / "main.ino.backup"
    
    if main_file.exists():
        shutil.copy(main_file, backup_file)
        print(f"✅ Backed up main.ino to {backup_file}")
        return True
    return False

def create_test_main():
    """Create a temporary main.ino that includes test_security.cpp"""
    src_dir = Path("src")
    test_file = Path("test") / "test_security.cpp"
    
    if not test_file.exists():
        print(f"❌ Test file not found: {test_file}")
        return False
    
    # Read test file content
    with open(test_file, 'r', encoding='utf-8') as f:
        test_content = f.read()
    
    # Write to main.ino temporarily
    main_file = src_dir / "main.ino"
    with open(main_file, 'w', encoding='utf-8') as f:
        f.write(test_content)
    
    print(f"✅ Created test main.ino from {test_file}")
    return True

def restore_main_file():
    """Restore original main.ino"""
    src_dir = Path("src")
    backup_dir = Path(".backup")
    
    backup_file = backup_dir / "main.ino.backup"
    main_file = src_dir / "main.ino"
    
    if backup_file.exists():
        shutil.copy(backup_file, main_file)
        print(f"✅ Restored main.ino from backup")
        return True
    return False

def main():
    print("=" * 60)
    print("🔐 Security Layer Standalone Test Setup")
    print("=" * 60)
    print()
    
    print("This script will:")
    print("  1. Backup your current main.ino")
    print("  2. Replace it with test_security.cpp")
    print("  3. Let you upload and test")
    print("  4. Restore original main.ino when done")
    print()
    
    choice = input("Continue? (y/n): ").strip().lower()
    if choice != 'y':
        print("❌ Cancelled")
        return
    
    # Backup
    if not backup_main_file():
        print("❌ Failed to backup main.ino")
        return
    
    # Create test version
    if not create_test_main():
        print("❌ Failed to create test main.ino")
        restore_main_file()
        return
    
    print()
    print("✅ Setup complete!")
    print()
    print("Next steps:")
    print("  1. Run: pio run -t upload -t monitor")
    print("  2. Watch the test results in serial monitor")
    print("  3. After testing, run this script again with '--restore' flag")
    print()
    print("To restore original main.ino:")
    print("  python run_security_test.py --restore")
    print()

if __name__ == "__main__":
    import sys
    
    if "--restore" in sys.argv:
        print("🔄 Restoring original main.ino...")
        restore_main_file()
        print("✅ Restoration complete!")
    else:
        main()
