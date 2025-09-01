#!/usr/bin/env python3
"""
Pytest Configuration for SaaS Project
Sets up test fixtures and configuration
"""

import pytest
import os
import sys
from pathlib import Path

# Add project root to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


@pytest.fixture(scope="session")
def project_root():
    """Return the project root directory"""
    return Path(__file__).parent.parent


@pytest.fixture(scope="session")
def scripts_dir(project_root):
    """Return the scripts directory"""
    return project_root / "scripts"


@pytest.fixture(scope="session")
def config_dir(project_root):
    """Return the config directory"""
    return project_root / "config"


@pytest.fixture(scope="session")
def test_data_dir():
    """Return the test data directory"""
    return Path(__file__).parent / "data"


@pytest.fixture(scope="session")
def sample_env_vars():
    """Return sample environment variables for testing"""
    return {
        "DB_HOST": "localhost",
        "DB_PORT": "5432",
        "DB_NAME": "saas_test",
        "DB_USER": "test_user",
        "DB_PASSWORD": "test_password",
        "JWT_SECRET": "test_jwt_secret",
        "POSTGREST_PORT": "3000",
    }


# Test configuration
def pytest_configure(config):
    """Configure pytest with custom markers"""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )
    config.addinivalue_line("markers", "integration: marks tests as integration tests")
    config.addinivalue_line("markers", "unit: marks tests as unit tests")


def pytest_collection_modifyitems(config, items):
    """Automatically mark tests based on their names"""
    for item in items:
        if "integration" in item.name.lower():
            item.add_marker(pytest.mark.integration)
        elif "unit" in item.name.lower():
            item.add_marker(pytest.mark.unit)
        else:
            item.add_marker(pytest.mark.unit)
