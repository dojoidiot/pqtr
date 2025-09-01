#!/usr/bin/env python3
"""
Script Tests for PITS Project
Tests the management and utility scripts
"""

import pytest
import os
import sys
from pathlib import Path

# Add project root to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


class TestScripts:
    """Test the management scripts"""

    def test_pits_script_exists(self):
        """Test that the main pits script exists"""
        script_path = Path(__file__).parent.parent / "bin" / "pits.sh"
        assert script_path.exists(), f"PITS script not found at {script_path}"

    def test_init_script_exists(self):
        """Test that the init script exists"""
        script_path = Path(__file__).parent.parent / "bin" / "init.sh"
        assert script_path.exists(), f"Init script not found at {script_path}"

    def test_scripts_are_executable(self):
        """Test that all scripts are executable"""
        bin_dir = Path(__file__).parent.parent / "bin"
        for script in bin_dir.glob("*.sh"):
            assert os.access(script, os.X_OK), f"Script {script} is not executable"

    def test_script_permissions(self):
        """Test that scripts have correct permissions"""
        bin_dir = Path(__file__).parent.parent / "bin"
        for script in bin_dir.glob("*.sh"):
            stat = script.stat()
            # Check that scripts are executable by owner
            assert stat.st_mode & 0o100, f"Script {script} is not executable by owner"


class TestProjectStructure:
    """Test the project directory structure"""

    def test_bin_directory_exists(self):
        """Test that bin directory exists"""
        bin_dir = Path(__file__).parent.parent / "bin"
        assert bin_dir.exists(), f"Bin directory not found at {bin_dir}"

    def test_etc_directory_exists(self):
        """Test that etc directory exists"""
        etc_dir = Path(__file__).parent.parent / "etc"
        assert etc_dir.exists(), f"Etc directory not found at {etc_dir}"

    def test_www_directory_exists(self):
        """Test that www directory exists"""
        www_dir = Path(__file__).parent.parent / "www"
        assert www_dir.exists(), f"Www directory not found at {www_dir}"

    def test_doc_directory_exists(self):
        """Test that doc directory exists"""
        doc_dir = Path(__file__).parent.parent / "doc"
        assert doc_dir.exists(), f"Doc directory not found at {doc_dir}"


class TestConfiguration:
    """Test configuration files"""

    def test_config_file_exists(self):
        """Test that pits.conf exists"""
        config_path = Path(__file__).parent.parent / "etc" / "pits.conf"
        assert config_path.exists(), f"Config file not found at {config_path}"

    def test_readme_exists(self):
        """Test that README.md exists"""
        readme_path = Path(__file__).parent.parent / "README.md"
        assert readme_path.exists(), f"README not found at {readme_path}"


class TestWebInterface:
    """Test web interface components"""

    def test_index_html_exists(self):
        """Test that index.html exists"""
        index_path = Path(__file__).parent.parent / "www" / "index.html"
        assert index_path.exists(), f"Index HTML not found at {index_path}"

    def test_web_content_is_readable(self):
        """Test that web content is readable"""
        www_dir = Path(__file__).parent.parent / "www"
        for file in www_dir.glob("*"):
            if file.is_file():
                assert os.access(file, os.R_OK), f"Web file {file} is not readable"


if __name__ == "__main__":
    pytest.main([__file__])
