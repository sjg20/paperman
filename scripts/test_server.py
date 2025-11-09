#!/usr/bin/env python3
"""
Automated test suite for paperman-server REST API
"""

import sys
import requests
import json
from typing import Dict, Optional
import argparse

# Colors for terminal output
class Colors:
    GREEN = '\033[0;32m'
    RED = '\033[0;31m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

class TestRunner:
    def __init__(self, server_url: str, api_key: Optional[str] = None):
        self.server_url = server_url.rstrip('/')
        self.api_key = api_key
        self.tests_run = 0
        self.tests_passed = 0
        self.tests_failed = 0

    def _get_headers(self) -> Dict[str, str]:
        """Build headers with API key if provided"""
        headers = {}
        if self.api_key:
            headers['X-API-Key'] = self.api_key
        return headers

    def print_header(self, text: str):
        """Print a section header"""
        print()
        print("=" * 50)
        print(text)
        print("=" * 50)

    def print_test(self, name: str):
        """Print test name"""
        print(f"  Testing: {name} ... ", end='', flush=True)
        self.tests_run += 1

    def pass_test(self, note: str = ""):
        """Mark test as passed"""
        print(f"{Colors.GREEN}PASS{Colors.NC}")
        if note:
            print(f"    {note}")
        self.tests_passed += 1

    def fail_test(self, error: str):
        """Mark test as failed"""
        print(f"{Colors.RED}FAIL{Colors.NC}")
        print(f"    Error: {error}")
        self.tests_failed += 1

    def warn_test(self, warning: str):
        """Mark test with warning"""
        print(f"{Colors.YELLOW}WARN{Colors.NC}")
        print(f"    Warning: {warning}")

    def test_status(self):
        """Test /status endpoint"""
        self.print_header("Testing /status endpoint")

        # Test 1: Returns 200
        self.print_test("GET /status returns 200")
        try:
            response = requests.get(f"{self.server_url}/status", headers=self._get_headers())
            if response.status_code == 200:
                self.pass_test()
            else:
                self.fail_test(f"Expected 200, got {response.status_code}")
                return
        except Exception as e:
            self.fail_test(str(e))
            return

        # Test 2: Valid JSON with success field
        self.print_test("Response contains valid JSON")
        try:
            data = response.json()
            if 'success' in data:
                self.pass_test()
            else:
                self.warn_test("Missing success field")
        except Exception as e:
            self.fail_test(f"Invalid JSON: {e}")

        # Test 3: Contains server info
        self.print_test("Response contains server info")
        if 'server' in data:
            self.pass_test(f"Server: {data.get('server', 'unknown')}")
        else:
            self.warn_test("Missing server field")

    def test_repos(self):
        """Test /repos endpoint"""
        self.print_header("Testing /repos endpoint")

        # Test 1: Returns 200
        self.print_test("GET /repos returns 200")
        try:
            response = requests.get(f"{self.server_url}/repos", headers=self._get_headers())
            if response.status_code == 200:
                self.pass_test()
            else:
                self.fail_test(f"Expected 200, got {response.status_code}")
                return
        except Exception as e:
            self.fail_test(str(e))
            return

        # Test 2: Contains repositories
        self.print_test("Response contains repositories array")
        try:
            data = response.json()
            if 'repositories' in data:
                self.pass_test()
            else:
                self.fail_test("Missing repositories field")
                return
        except Exception as e:
            self.fail_test(f"Invalid JSON: {e}")
            return

        # Test 3: Contains count
        self.print_test("Response contains count")
        count = data.get('count', -1)
        if count >= 0:
            self.pass_test(f"Found {count} repository(ies)")
        else:
            self.fail_test("Invalid or missing count")

    def test_search(self):
        """Test /search endpoint"""
        self.print_header("Testing /search endpoint")

        # Test 1: Without query returns 400
        self.print_test("GET /search without query returns 400")
        try:
            response = requests.get(f"{self.server_url}/search", headers=self._get_headers())
            if response.status_code == 400:
                self.pass_test()
            else:
                self.fail_test(f"Expected 400, got {response.status_code}")
        except Exception as e:
            self.fail_test(str(e))

        # Test 2: With query returns 200
        self.print_test("GET /search?q=test returns 200")
        try:
            response = requests.get(f"{self.server_url}/search?q=test", headers=self._get_headers())
            if response.status_code == 200:
                self.pass_test()
            else:
                self.fail_test(f"Expected 200, got {response.status_code}")
                return
        except Exception as e:
            self.fail_test(str(e))
            return

        # Test 3: Contains results
        self.print_test("Search response contains results array")
        try:
            data = response.json()
            if 'results' in data:
                self.pass_test()
            else:
                self.fail_test("Missing results field")
                return
        except Exception as e:
            self.fail_test(f"Invalid JSON: {e}")
            return

        # Test 4: Contains count
        self.print_test("Search response contains count")
        count = data.get('count', -1)
        if count >= 0:
            self.pass_test(f"Found {count} file(s)")
        else:
            self.fail_test("Invalid or missing count")

        # Test 5: Recursive parameter
        self.print_test("Search with recursive parameter")
        try:
            response = requests.get(f"{self.server_url}/search?q=test&recursive=true", headers=self._get_headers())
            data = response.json()
            if data.get('success'):
                self.pass_test()
            else:
                self.fail_test("Recursive search failed")
        except Exception as e:
            self.fail_test(str(e))

        # Test 6: Path parameter
        self.print_test("Search with path parameter")
        try:
            response = requests.get(f"{self.server_url}/search?q=test&path=.", headers=self._get_headers())
            data = response.json()
            if data.get('success') is not None:  # Can be true or false
                self.pass_test()
            else:
                self.fail_test("Path search failed")
        except Exception as e:
            self.fail_test(str(e))

    def test_browse(self):
        """Test /browse endpoint"""
        self.print_header("Testing /browse endpoint")

        # Test 1: Returns 200
        self.print_test("GET /browse returns 200")
        try:
            response = requests.get(f"{self.server_url}/browse?path=", headers=self._get_headers())
            if response.status_code == 200:
                self.pass_test()
            else:
                self.fail_test(f"Expected 200, got {response.status_code}")
                return
        except Exception as e:
            self.fail_test(str(e))
            return

        # Test 2: Contains directories
        self.print_test("Browse response contains directories array")
        try:
            data = response.json()
            if 'directories' in data:
                self.pass_test()
            else:
                self.fail_test("Missing directories field")
                return
        except Exception as e:
            self.fail_test(f"Invalid JSON: {e}")
            return

        # Test 3: Contains files
        self.print_test("Browse response contains files array")
        if 'files' in data:
            self.pass_test()
        else:
            self.fail_test("Missing files field")

        # Test 4: Contains counts
        self.print_test("Browse response contains counts")
        dir_count = data.get('dirCount', -1)
        file_count = data.get('fileCount', -1)
        if dir_count >= 0 and file_count >= 0:
            self.pass_test(f"Found {dir_count} dir(s) and {file_count} file(s)")
        else:
            self.fail_test("Invalid counts")

        # Test 5: Directory traversal protection
        self.print_test("Browse with directory traversal attack")
        try:
            response = requests.get(f"{self.server_url}/browse?path=../../../etc", headers=self._get_headers())
            if response.status_code in [400, 404]:
                self.pass_test()
            else:
                self.warn_test(f"Directory traversal not properly blocked (got {response.status_code})")
        except Exception as e:
            self.fail_test(str(e))

    def test_list(self):
        """Test /list endpoint"""
        self.print_header("Testing /list endpoint")

        self.print_test("GET /list returns 200")
        try:
            response = requests.get(f"{self.server_url}/list?path=", headers=self._get_headers())
            if response.status_code == 200:
                self.pass_test()
            else:
                self.fail_test(f"Expected 200, got {response.status_code}")
        except Exception as e:
            self.fail_test(str(e))

    def test_file(self):
        """Test /file endpoint"""
        self.print_header("Testing /file endpoint")

        # Test 1: Without path returns 400
        self.print_test("GET /file without path returns 400")
        try:
            response = requests.get(f"{self.server_url}/file", headers=self._get_headers())
            if response.status_code == 400:
                self.pass_test()
            else:
                self.fail_test(f"Expected 400, got {response.status_code}")
        except Exception as e:
            self.fail_test(str(e))

        # Test 2: Nonexistent file returns 404
        self.print_test("GET /file with nonexistent file returns 404")
        try:
            response = requests.get(f"{self.server_url}/file?path=nonexistent.max", headers=self._get_headers())
            if response.status_code == 404:
                self.pass_test()
            else:
                self.fail_test(f"Expected 404, got {response.status_code}")
        except Exception as e:
            self.fail_test(str(e))

        # Test 3: Directory traversal protection
        self.print_test("GET /file with directory traversal returns 400")
        try:
            response = requests.get(f"{self.server_url}/file?path=../../etc/passwd", headers=self._get_headers())
            if response.status_code in [400, 404]:
                self.pass_test()
            else:
                self.warn_test(f"Directory traversal not properly blocked (got {response.status_code})")
        except Exception as e:
            self.fail_test(str(e))

        # Test 4: Invalid type parameter
        self.print_test("GET /file with invalid type returns 400")
        try:
            response = requests.get(f"{self.server_url}/file?path=test.max&type=invalid", headers=self._get_headers())
            if response.status_code == 400:
                self.pass_test()
            else:
                self.fail_test(f"Expected 400, got {response.status_code}")
        except Exception as e:
            self.fail_test(str(e))

    def test_auth(self):
        """Test authentication"""
        if not self.api_key:
            print("\nSkipping authentication tests (no API_KEY set)")
            return

        self.print_header("Testing authentication")

        # Test 1: Request without API key
        self.print_test("Request without API key returns 401")
        try:
            response = requests.get(f"{self.server_url}/search?q=test")
            if response.status_code == 401:
                self.pass_test()
            else:
                self.warn_test(f"Expected 401, got {response.status_code} (auth might be disabled)")
        except Exception as e:
            self.fail_test(str(e))

        # Test 2: Request with invalid API key
        self.print_test("Request with invalid API key returns 401")
        try:
            response = requests.get(f"{self.server_url}/search?q=test", headers={'X-API-Key': 'invalid-key'})
            if response.status_code == 401:
                self.pass_test()
            else:
                self.warn_test(f"Expected 401, got {response.status_code}")
        except Exception as e:
            self.fail_test(str(e))

        # Test 3: Status endpoint without auth
        self.print_test("/status endpoint works without API key")
        try:
            response = requests.get(f"{self.server_url}/status")
            if response.status_code == 200:
                self.pass_test()
            else:
                self.fail_test("Status endpoint should be accessible without auth")
        except Exception as e:
            self.fail_test(str(e))

    def test_errors(self):
        """Test error handling"""
        self.print_header("Testing error handling")

        # Test 1: Invalid endpoint
        self.print_test("Invalid endpoint returns 404")
        try:
            response = requests.get(f"{self.server_url}/invalid-endpoint", headers=self._get_headers())
            if response.status_code == 404:
                self.pass_test()
            else:
                self.fail_test(f"Expected 404, got {response.status_code}")
        except Exception as e:
            self.fail_test(str(e))

        # Test 2: Malformed request
        self.print_test("Malformed request handled gracefully")
        try:
            response = requests.get(f"{self.server_url}/search?q=%ZZ", headers=self._get_headers())
            if 200 <= response.status_code < 500:
                self.pass_test()
            else:
                self.fail_test(f"Server error on malformed request (got {response.status_code})")
        except Exception as e:
            self.fail_test(str(e))

    def run_all_tests(self):
        """Run all test suites"""
        print(f"{Colors.BLUE}Paperman Server Test Suite{Colors.NC}")
        print(f"Server URL: {self.server_url}")
        print(f"API Key: {'[set]' if self.api_key else '[not set]'}")

        # Check if server is running
        try:
            response = requests.get(f"{self.server_url}/status", timeout=5)
        except Exception as e:
            print(f"\n{Colors.RED}Error: Server at {self.server_url} is not responding{Colors.NC}")
            print("Make sure paperman-server is running")
            return False

        # Run all tests
        self.test_status()
        self.test_repos()
        self.test_search()
        self.test_browse()
        self.test_list()
        self.test_file()
        self.test_auth()
        self.test_errors()

        # Print summary
        self.print_header("Test Summary")
        print(f"Total tests run: {self.tests_run}")
        print(f"Passed: {Colors.GREEN}{self.tests_passed}{Colors.NC}")
        print(f"Failed: {Colors.RED}{self.tests_failed}{Colors.NC}")

        if self.tests_failed == 0:
            print(f"\n{Colors.GREEN}All tests passed!{Colors.NC}")
            return True
        else:
            print(f"\n{Colors.RED}Some tests failed{Colors.NC}")
            return False

def main():
    parser = argparse.ArgumentParser(description="Test paperman-server REST API")
    parser.add_argument("--url", default="http://localhost:8080",
                       help="Server URL (default: http://localhost:8080)")
    parser.add_argument("--api-key", default="",
                       help="API key for authentication")

    args = parser.parse_args()

    runner = TestRunner(args.url, args.api_key or None)
    success = runner.run_all_tests()

    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
