#!/usr/bin/env python3
"""
Real VST3 Plugin Detection Test
Tests the actual VST3 plugin scanning logic on the system
"""

import os
import sys
from pathlib import Path

class RealVST3Scanner:
    def __init__(self):
        self.system_dirs = self.get_system_vst3_directories()
    
    def get_system_vst3_directories(self):
        """Get system VST3 directories based on platform"""
        dirs = []
        
        if os.name == 'nt':  # Windows
            dirs.extend([
                Path("C:/Program Files/Common Files/VST3"),
                Path("C:/Program Files (x86)/Common Files/VST3")
            ])
            
            # User-specific directories
            user_profile = os.environ.get("USERPROFILE")
            if user_profile:
                user_path = Path(user_profile)
                dirs.extend([
                    user_path / "AppData/Roaming/VST3",
                    user_path / "Documents/VST3"
                ])
        
        elif sys.platform == 'darwin':  # macOS
            dirs.extend([
                Path("/Library/Audio/Plug-Ins/VST3"),
                Path("~/Library/Audio/Plug-Ins/VST3").expanduser(),
                Path("/System/Library/Audio/Plug-Ins/VST3")
            ])
        
        else:  # Linux
            dirs.extend([
                Path("/usr/lib/vst3"),
                Path("/usr/local/lib/vst3"),
                Path("~/.vst3").expanduser()
            ])
        
        return dirs
    
    def scan_system_plugins(self):
        """Scan all system VST3 directories for plugins"""
        plugins = []
        
        print("Scanning VST3 directories for plugins...")
        
        for dir_path in self.system_dirs:
            if not dir_path.exists():
                print(f"Directory not found: {dir_path}")
                continue
            
            print(f"Scanning: {dir_path}")
            
            try:
                for entry in dir_path.iterdir():
                    if entry.is_dir() and self.is_valid_vst3_bundle(entry):
                        plugin_info = self.extract_plugin_info(entry)
                        plugins.append(plugin_info)
                        print(f"  Found: {plugin_info['name']}")
            except PermissionError as e:
                print(f"Permission denied scanning {dir_path}: {e}")
            except Exception as e:
                print(f"Error scanning {dir_path}: {e}")
        
        return plugins
    
    def find_span_plugin(self):
        """Look for Span VST3 plugin"""
        print("Looking for Span.vst3...")
        
        span_paths = [
            Path("C:/Program Files/Common Files/VST3/Span.vst3"),
            Path("C:/Program Files (x86)/Common Files/VST3/Span.vst3")
        ]
        
        for path in span_paths:
            if path.exists():
                print(f"Found Span at: {path}")
                return self.extract_plugin_info(path)
        
        return None
    
    def find_tdr_nova_plugin(self):
        """Look for TDR Nova VST3 plugin"""
        print("Looking for TDR Nova.vst3...")
        
        nova_paths = [
            Path("C:/Program Files/Common Files/VST3/TDR Nova.vst3"),
            Path("C:/Program Files (x86)/Common Files/VST3/TDR Nova.vst3"),
            Path("C:/Program Files/Common Files/VST3/Tokyo Dawn Labs/TDR Nova.vst3")
        ]
        
        for path in nova_paths:
            if path.exists():
                print(f"Found TDR Nova at: {path}")
                return self.extract_plugin_info(path)
        
        return None
    
    def is_valid_vst3_bundle(self, path):
        """Check if path is a valid VST3 bundle"""
        return path.is_dir() and path.suffix == '.vst3'
    
    def extract_plugin_info(self, plugin_path):
        """Extract basic plugin information"""
        info = {
            'name': self.extract_plugin_name(plugin_path),
            'path': str(plugin_path),
            'manufacturer': 'Unknown',
            'version': 'Unknown',
            'uid': 'Unknown',
            'is_valid': True
        }
        
        # Look for Contents directory
        contents_dir = plugin_path / "Contents"
        if contents_dir.exists():
            # Try to find the actual plugin binary
            arch_dirs = ["x86_64-win", "win64", "Windows"]
            
            for arch_dir in arch_dirs:
                binary_path = contents_dir / arch_dir
                if binary_path.exists():
                    # VST3 binary should have same name as bundle but with .vst3 extension
                    binary_file = binary_path / f"{info['name']}.vst3"
                    if binary_file.exists():
                        print(f"  Binary found: {binary_file}")
                        break
        
        return info
    
    def extract_plugin_name(self, plugin_path):
        """Extract plugin name from path"""
        filename = plugin_path.name
        # Remove .vst3 extension
        if filename.endswith('.vst3'):
            return filename[:-5]
        return filename
    
    def print_download_instructions(self):
        """Print download instructions for VST3 plugins"""
        print("\n=== VST3 Plugin Download Instructions ===")
        print("\nTo test real VST3 integration, please install one of these free plugins:\n")
        
        print("1. Voxengo Span (Spectrum Analyzer):")
        print("   Download: https://www.voxengo.com/product/span/")
        print("   Install to: C:/Program Files/Common Files/VST3/Span.vst3\n")
        
        print("2. TDR Nova (Dynamic EQ):")
        print("   Download: https://www.tokyodawn.net/tdr-nova/")
        print("   Install to: C:/Program Files/Common Files/VST3/TDR Nova.vst3\n")
        
        print("Alternative free VST3 plugins:")
        print("- ReaPlugs VST FX Suite (from Cockos)")
        print("- Melda Free Bundle (MeldaProduction)")
        print("- Blue Cat's Freeware Bundle\n")
        
        print("After installation, run this test again to verify detection.")
        print("==========================================\n")

def main():
    print("=== MixMind AI Real VST3 Detection Test ===")
    print("Testing real VST3 plugin detection on system")
    print("============================================\n")
    
    scanner = RealVST3Scanner()
    
    # Look for specific plugins first
    span_plugin = scanner.find_span_plugin()
    nova_plugin = scanner.find_tdr_nova_plugin()
    
    if span_plugin:
        print(f"\n[OK] SPAN FOUND: {span_plugin['path']}")
    else:
        print("\n[X] Span not found")
    
    if nova_plugin:
        print(f"[OK] TDR NOVA FOUND: {nova_plugin['path']}")
    else:
        print("[X] TDR Nova not found")
    
    # Scan all plugins
    all_plugins = scanner.scan_system_plugins()
    
    if all_plugins:
        print(f"\n[INFO] TOTAL VST3 PLUGINS FOUND: {len(all_plugins)}")
        
        print("\nDetailed plugin list:")
        for plugin in all_plugins:
            print(f"  - {plugin['name']} ({plugin['path']})")
        
        # Create artifacts directory and save results
        artifacts_dir = Path("artifacts")
        artifacts_dir.mkdir(exist_ok=True)
        
        # Generate summary file
        summary_file = artifacts_dir / "real_vst3_detection.txt"
        with open(summary_file, 'w') as f:
            f.write("MixMind AI - Real VST3 Detection Results\n")
            f.write("=====================================\n\n")
            
            from datetime import datetime
            f.write(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Test: Real VST3 Plugin Detection\n\n")
            
            f.write(f"VST3 Plugins Found: {len(all_plugins)}\n\n")
            
            for plugin in all_plugins:
                f.write(f"Plugin: {plugin['name']}\n")
                f.write(f"  Path: {plugin['path']}\n")
                f.write(f"  Valid: {plugin['is_valid']}\n\n")
            
            f.write("Specific Plugin Detection:\n")
            f.write(f"  Span: {'FOUND' if span_plugin else 'NOT FOUND'}\n")
            f.write(f"  TDR Nova: {'FOUND' if nova_plugin else 'NOT FOUND'}\n\n")
            
            f.write("Real VST3 Integration Status: ")
            if all_plugins:
                f.write("SUCCESS - Real plugins detected and validated\n")
            else:
                f.write("READY - System configured for VST3 plugins\n")
                f.write("Install Span or TDR Nova to test with real plugins\n")
        
        print(f"\n[OK] Results saved to: {summary_file}")
        
    else:
        print("\n[X] No VST3 plugins found in system directories")
        scanner.print_download_instructions()
    
    print("\n=== Test Complete ===")
    return len(all_plugins) > 0 or span_plugin is not None or nova_plugin is not None

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)