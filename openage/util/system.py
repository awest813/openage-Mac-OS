# Copyright 2015-2026 the openage authors. See copying.md for legal info.

"""
Various OS utilities
"""

import re
import subprocess
from sys import platform

from .math import INF


def free_memory() -> int:
    """
    Returns the amount of free bytes of memory.
    On failure, returns +inf.

    >>> free_memory() > 0
    True
    """
    memory = INF

    if platform.startswith('linux'):
        pattern = re.compile('^MemAvailable: +([0-9]+) kB\n$')
        with open('/proc/meminfo', encoding='utf8') as meminfo:
            for line in meminfo:
                match = pattern.match(line)
                if match:
                    memory = 1024 * int(match.group(1))
                    break
    elif platform == "darwin":
        memory = _free_memory_macos()
    elif platform == "win32":
        # TODO
        pass

    return memory


def _free_memory_macos() -> int:
    """
    Query free memory on macOS via vm_stat.

    Parses 'Pages free' and 'Pages speculative' from vm_stat output
    and multiplies by the reported page size to obtain free bytes.

    Returns +inf on any failure so the caller can proceed safely.
    """
    try:
        output = subprocess.check_output(
            ["vm_stat"],
            text=True,
            timeout=5,
        )
    except (subprocess.SubprocessError, FileNotFoundError):
        return INF

    # First line contains the page size, e.g.
    #   "Mach Virtual Memory Statistics: (page size of 16384 bytes)"
    page_size_match = re.search(r'page size of (\d+) bytes', output)
    if not page_size_match:
        return INF

    page_size = int(page_size_match.group(1))

    # Sum the page counts that represent usable free memory.
    free_pages = 0
    for key in ("Pages free", "Pages speculative"):
        match = re.search(rf'^{re.escape(key)}:\s+(\d+)\.$', output, re.MULTILINE)
        if match:
            free_pages += int(match.group(1))

    if free_pages == 0:
        return INF

    return page_size * free_pages
