#!/usr/bin/env python3
"""
Script Tests for SaaS Project
Tests the management and utility scripts
"""

import pytest
import subprocess
import os
import sys
from pathlib import Path

# Add project root to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

class TestScripts:
    """Test the management scripts"""
    
    def test_project_script_exists(self):
        """Test that the main project script exists"""
        script_path = Path(__file__).parent.parent / "bin" / "project.sh"
        assert script_path.exists(), f"Project script not found at {script_path}"
    
    def test_setup_script_exists(self):
        """Test that the setup script exists"""
        script_path = Path(__file__).parent.parent / "bin" / "setup-local.sh"
        assert script_path.exists(), f"Setup script not found at {script_path}"
    
    def test_quick_start_script_exists(self):
        """Test that the quick start script exists"""
        script_path = Path(__file__).parent.parent / "quick-start.sh"
        assert script_path.exists(), f"Quick start script not found at {script_path}"
    
    def test_scripts_are_executable(self):
        """Test that all scripts are executable"""
        bin_dir = Path(__file__).parent.parent / "bin"
        for script in bin_dir.glob("*.sh"):
            assert os.access(script, os.X_OK), f"Script {script} is not executable"

class TestConfiguration:
    """Test configuration files and structure"""
    
    def test_project_conf_exists(self):
        """Test that project.conf exists"""
        conf_path = Path(__file__).parent.parent / "project.conf"
        assert conf_path.exists(), f"Project config not found at {conf_path}"
    
    def test_env_example_exists(self):
        """Test that env.example exists"""
        env_path = Path(__file__).parent.parent / "env.example"
        assert env_path.exists(), f"Environment example not found at {env_path}"
    
    def test_config_directory_exists(self):
        """Test that config directory exists"""
        config_dir = Path(__file__).parent.parent / "etc"
        assert config_dir.exists(), f"Config directory not found at {config_dir}"

class TestDocumentation:
    """Test documentation files"""
    
    def test_readme_exists(self):
        """Test that README.md exists"""
        readme_path = Path(__file__).parent.parent / "README.md"
        assert readme_path.exists(), f"README not found at {readme_path}"
    
    def test_deployment_doc_exists(self):
        """Test that DEPLOYMENT.md exists"""
        deploy_path = Path(__file__).parent.parent / "DEPLOYMENT.md"
        assert deploy_path.exists(), f"Deployment doc not found at {deploy_path}"
    
    def test_status_doc_exists(self):
        """Test that PROJECT_STATUS.md exists"""
        status_path = Path(__file__).parent.parent / "PROJECT_STATUS.md"
        assert status_path.exists(), f"Status doc not found at {status_path}"

if __name__ == "__main__":
    pytest.main([__file__])
