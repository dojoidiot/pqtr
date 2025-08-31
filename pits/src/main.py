#!/usr/bin/env python3
"""
PITS Project - Main Application
A sample Python application for the Picture Image Transfer System.
"""

import os
import sys
import json
from pathlib import Path

class PITSProject:
    """Main PITS Picture Image Transfer System class."""
    
    def __init__(self):
        self.project_name = "PITS"
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
    
    def create_directories(self):
        """Create necessary directories if they don't exist."""
        directories = ['var/logs', 'var/cache', 'var/tmp', 'var/build']
        
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
            elif command == "structure":
                print(json.dumps(self.get_project_structure(), indent=2))
            else:
                print(f"Unknown command: {command}")
                print("Available commands: status, init, structure")
        else:
            self.show_status()

def main():
    """Main entry point."""
            project = PITSProject()
    project.run()

if __name__ == "__main__":
    main()
