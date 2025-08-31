#!/usr/bin/env python3
"""
Site Project - Main Application
A sample Python application for site management and deployment.
"""

import os
import sys
import json
import shutil
from pathlib import Path
from datetime import datetime

class SiteProject:
    """Main Site project class."""
    
    def __init__(self):
        self.project_name = "Site"
        self.version = "1.0.0"
        self.root_dir = Path(__file__).parent.parent
        
    def get_project_structure(self):
        """Get the current project directory structure."""
        structure = {
            'bin': self._list_directory('bin'),
            'etc': self._list_directory('etc'),
            'src': self._list_directory('src'),
            'var': self._list_directory('var'),
            'www': self._list_directory('www')
        }
        return structure
    
    def _list_directory(self, dir_name):
        """List contents of a directory."""
        dir_path = self.root_dir / dir_name
        if dir_path.exists() and dir_path.is_dir():
            return [item.name for item in dir_path.iterdir()]
        return []
    
    def show_status(self):
        """Display project status."""
        print(f"🚀 {self.project_name} v{self.version}")
        print("=" * 40)
        print(f"Root directory: {self.root_dir}")
        print()
        
        structure = self.get_project_structure()
        for dir_name, contents in structure.items():
            print(f"📁 {dir_name}/")
            for item in contents:
                print(f"   {item}")
            print()
        
        # Check build status
        build_dir = self.root_dir / "var" / "build"
        if build_dir.exists() and any(build_dir.iterdir()):
            print("📦 Build Status: Ready")
            print("   Build artifacts found in var/build/")
        else:
            print("📦 Build Status: Not built")
            print("   Run './bin/site build' to create build artifacts")
    
    def create_build(self):
        """Create build artifacts."""
        print("🔨 Creating build...")
        
        build_dir = self.root_dir / "var" / "build"
        build_dir.mkdir(parents=True, exist_ok=True)
        
        # Copy source files
        src_dir = self.root_dir / "src"
        if src_dir.exists():
            shutil.copytree(src_dir, build_dir / "src", dirs_exist_ok=True)
            print("   ✅ Source files copied")
        
        # Copy web files
        www_dir = self.root_dir / "www"
        if www_dir.exists():
            shutil.copytree(www_dir, build_dir / "www", dirs_exist_ok=True)
            print("   ✅ Web files copied")
        
        # Create build info
        build_info = {
            "project": self.project_name,
            "version": self.version,
            "build_time": datetime.now().isoformat(),
            "build_directory": str(build_dir)
        }
        
        with open(build_dir / "build-info.json", "w") as f:
            json.dump(build_info, f, indent=2)
        
        print("   ✅ Build info created")
        print(f"✅ Build completed: {build_dir}")
    
    def create_deployment_package(self):
        """Create deployment package."""
        print("🚀 Creating deployment package...")
        
        build_dir = self.root_dir / "var" / "build"
        if not build_dir.exists() or not any(build_dir.iterdir()):
            print("❌ No build artifacts found. Run 'build' first.")
            return
        
        deploy_dir = self.root_dir / "var" / "deploy"
        deploy_dir.mkdir(parents=True, exist_ok=True)
        
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        package_name = f"site-{timestamp}.tar.gz"
        package_path = deploy_dir / package_name
        
        # Create tar.gz package
        shutil.make_archive(
            str(package_path).replace('.tar.gz', ''),
            'gztar',
            build_dir
        )
        
        print(f"✅ Deployment package created: {package_path}")
        print(f"📦 Package size: {package_path.stat().st_size / 1024:.1f} KB")
    
    def create_directories(self):
        """Create necessary directories if they don't exist."""
        directories = [
            'var/logs', 'var/cache', 'var/tmp', 
            'var/build', 'var/deploy'
        ]
        
        for directory in directories:
            dir_path = self.root_dir / directory
            dir_path.mkdir(parents=True, exist_ok=True)
            print(f"✅ Created directory: {directory}")
    
    def run(self):
        """Main run method."""
        if len(sys.argv) > 1:
            command = sys.argv[1]
            
            if command == "status":
                self.show_status()
            elif command == "init":
                self.create_directories()
            elif command == "build":
                self.create_build()
            elif command == "deploy":
                self.create_deployment_package()
            elif command == "structure":
                print(json.dumps(self.get_project_structure(), indent=2))
            else:
                print(f"Unknown command: {command}")
                print("Available commands: status, init, build, deploy, structure")
        else:
            self.show_status()

def main():
    """Main entry point."""
    project = SiteProject()
    project.run()

if __name__ == "__main__":
    main()
