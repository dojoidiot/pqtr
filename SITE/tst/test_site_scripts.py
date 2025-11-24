#!/usr/bin/env python3
"""
Script Tests for Site Project
Tests the project management and build scripts
"""

import pytest
import os
import sys
from pathlib import Path


class TestScripts:
    """Test the management scripts"""

    def test_make_script_exists(self):
        """Test that the make script exists"""
        script_path = Path(__file__).parent.parent / "bin" / "make.sh"
        assert script_path.exists(), f"Make script not found at {script_path}"

    def test_send_script_exists(self):
        """Test that the send script exists"""
        script_path = Path(__file__).parent.parent / "bin" / "send.sh"
        assert script_path.exists(), f"Send script not found at {script_path}"

    def test_scripts_are_executable(self):
        """Test that all scripts are executable"""
        bin_dir = Path(__file__).parent.parent / "bin"
        for script in bin_dir.glob("*"):
            if script.is_file():
                assert os.access(script, os.X_OK), f"Script {script} is not executable"


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

    def test_src_directory_exists(self):
        """Test that src directory exists"""
        src_dir = Path(__file__).parent.parent / "src"
        assert src_dir.exists(), f"Src directory not found at {src_dir}"

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

    def test_pqtr_ai_conf_exists(self):
        """Test that pqtr.ai.conf exists"""
        config_path = Path(__file__).parent.parent / "etc" / "pqtr.ai.conf"
        assert config_path.exists(), f"PQTR AI config not found at {config_path}"

    def test_apex_md_exists(self):
        """Test that apex.md exists"""
        apex_path = Path(__file__).parent.parent / "doc" / "apex.md"
        assert apex_path.exists(), f"Apex.md not found at {apex_path}"


class TestSourceCode:
    """Test source code components"""

    def test_main_py_exists(self):
        """Test that main.py exists"""
        main_path = Path(__file__).parent.parent / "src" / "main.py"
        assert main_path.exists(), f"Main.py not found at {main_path}"

    def test_makefile_exists(self):
        """Test that Makefile exists"""
        makefile_path = Path(__file__).parent.parent / "src" / "Makefile"
        assert makefile_path.exists(), f"Makefile not found at {makefile_path}"

    def test_python_code_is_readable(self):
        """Test that Python source code is readable"""
        src_dir = Path(__file__).parent.parent / "src"
        for file in src_dir.glob("*.py"):
            assert os.access(file, os.R_OK), f"Python file {file} is not readable"


class TestWebContent:
    """Test web content"""

    def test_main_html_exists(self):
        """Test that main.html exists"""
        main_path = Path(__file__).parent.parent / "www" / "main.html"
        assert main_path.exists(), f"Main HTML not found at {main_path}"

    def test_main_png_exists(self):
        """Test that main.png exists"""
        png_path = Path(__file__).parent.parent / "www" / "main.png"
        assert png_path.exists(), f"Main PNG not found at {png_path}"

    def test_web_content_is_readable(self):
        """Test that web content is readable"""
        www_dir = Path(__file__).parent.parent / "www"
        for file in www_dir.glob("*"):
            if file.is_file():
                assert os.access(file, os.R_OK), f"Web file {file} is not readable"


if __name__ == "__main__":
    pytest.main([__file__])
