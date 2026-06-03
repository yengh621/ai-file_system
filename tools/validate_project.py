#!/usr/bin/env python3
from __future__ import annotations

import os
import json
import py_compile
import shutil
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path
import platform


# ========== 跨平台全局配置 ==========
OS_NAME = platform.system()
if OS_NAME == "Windows":
    BIN_NAME = "filesystem.exe"
    BUILD_CMD = ["powershell", "-ExecutionPolicy", "Bypass", "-File", "build.ps1"]
else:
    BIN_NAME = "filesystem"
    BUILD_CMD = ["make"]


ROOT = Path(__file__).resolve().parent.parent


def compile_python() -> list[str]:
    failures: list[str] = []
    for path in ROOT.rglob("*.py"):
        if "__pycache__" in path.parts:
            continue
        try:
            py_compile.compile(str(path), doraise=True)
        except Exception as exc:
            failures.append(f"{path}: {exc}")
    return failures


def build_filesystem() -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        BUILD_CMD,
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def prepare_env(tmp: Path) -> None:
    shutil.copy2(ROOT / BIN_NAME, tmp / BIN_NAME)
    shutil.copytree(ROOT / "ai", tmp / "ai")
    shutil.copy2(ROOT / "config.json", tmp / "config.json")


def run_fs(commands: str, cwd: Path) -> str:
    if OS_NAME == "Windows":
        exec_cmd = [BIN_NAME]
    else:
        exec_cmd = [f"./{BIN_NAME}"]

    output = subprocess.check_output(
        exec_cmd,
        input=textwrap.dedent(commands).lstrip().encode("utf-8"),
        cwd=cwd,
    )
    return output.decode("utf-8", errors="replace")


def validate_core_flow(tmp: Path) -> list[str]:
    text = run_fs(
        """
        login
        root
        123456
        mkdir demo
        chdir demo
        create note
        open note rw
        write 0 hello
        close 0
        open note r
        read 0 5
        close 0
        chdir ..
        delete missing
        chdir demo
        delete note
        chdir ..
        rmdir demo
        blocks
        logout
        exit
        """,
        tmp,
    )

    failures: list[str] = []
    checks = {
        "login": "Login successful." in text,
        "mkdir": "Mkdir successful." in text,
        "create": "Create successful." in text,
        "write": "Write 5 bytes." in text,
        "read": "Content: hello" in text,
        "delete": "Delete successful." in text,
        "rmdir": "Rmdir successful." in text,
        "blocks": "BLOCK_STATUS:" in text,
        "logout": "Logout successful." in text,
    }
    for name, ok in checks.items():
        if not ok:
            failures.append(f"core_flow:{name}")
    return failures


def validate_link_flow(tmp: Path) -> list[str]:
    text = run_fs(
        """
        login
        root
        123456
        create note
        link note hard1
        unlink hard1
        symlink /note soft1
        readlink soft1
        unlink soft1
        delete note
        logout
        exit
        """,
        tmp,
    )

    checks = {
        "hard_link": "link: hard1 -> note (hard link)" in text,
        "hard_unlink": "unlink: hard1 removed" in text or "unlink: hard1 deleted" in text,
        "sym_link": "symlink: soft1 -> /note (symbolic link)" in text,
        "readlink": "readlink: /note" in text,
        "sym_unlink": "unlink: soft1 removed" in text or "unlink: soft1 deleted" in text,
    }
    return [f"link_flow:{name}" for name, ok in checks.items() if not ok]


def validate_copy_move_flow(tmp: Path) -> list[str]:
    text = run_fs(
        """
        login
        root
        123456
        create note
        open note w
        write 0 hello
        close 0
        mkdir docs
        copy /note /docs/note_copy
        move /docs/note_copy /docs/note_moved
        chdir /docs
        open note_moved r
        read 0 16
        close 0
        chdir /
        delete note
        chdir /docs
        delete note_moved
        chdir /
        rmdir docs
        logout
        exit
        """,
        tmp,
    )

    checks = {
        "copy": "Copy successful: /note -> /docs/note_copy" in text,
        "move": "Move successful: /docs/note_copy -> /docs/note_moved" in text,
        "content": "Content: hello" in text,
    }
    return [f"copy_move_flow:{name}" for name, ok in checks.items() if not ok]


def validate_cli_flow(tmp: Path) -> list[str]:
    failures: list[str] = []
    commands = [
        ["python", "-m", "ai.cli", "set_user", "0"],
        ["python", "-m", "ai.cli", "record", "create", "sample.txt"],
        ["python", "-m", "ai.cli", "get_optimization"],
        ["python", "-m", "ai.cli", "get_last_analysis"],
        ["python", "-m", "ai.cli", "clear_user"],
    ]
    for cmd in commands:
        result = subprocess.run(cmd, cwd=tmp, text=True, capture_output=True, check=False)
        if result.returncode != 0:
            failures.append(f"cli_flow:returncode:{' '.join(cmd)}")
            continue
        if not result.stdout.strip():
            failures.append(f"cli_flow:empty:{' '.join(cmd)}")
    return failures


def validate_user_memory_isolation(tmp: Path) -> list[str]:
    failures: list[str] = []
    commands = [
        ["python", "-m", "ai.cli", "set_user", "0"],
        ["python", "-m", "ai.cli", "record", "create", "root.txt"],
        ["python", "-m", "ai.cli", "set_user", "100"],
        ["python", "-m", "ai.cli", "record", "delete", "usr1.txt"],
    ]
    for cmd in commands:
        result = subprocess.run(cmd, cwd=tmp, text=True, capture_output=True, check=False)
        if result.returncode != 0:
            failures.append(f"user_memory:returncode:{' '.join(cmd)}")

    try:
        root_ops = json.loads(
            (tmp / "debug_memory/users/0/agent/memory/long_term/all_operations.json").read_text(encoding="utf-8")
        )
        user_ops = json.loads(
            (tmp / "debug_memory/users/100/agent/memory/long_term/all_operations.json").read_text(encoding="utf-8")
        )
    except Exception as exc:
        return failures + [f"user_memory:read:{exc}"]

    if not root_ops or root_ops[-1].get("uid") != 0 or root_ops[-1].get("path") != "root.txt":
        failures.append("user_memory:root_scope")
    if not user_ops or user_ops[-1].get("uid") != 100 or user_ops[-1].get("path") != "usr1.txt":
        failures.append("user_memory:user_scope")
    if any(op.get("uid") == 100 for op in root_ops):
        failures.append("user_memory:root_contaminated")
    if any(op.get("uid") == 0 for op in user_ops):
        failures.append("user_memory:user_contaminated")
    return failures


def validate_agent_stats_persistence(tmp: Path) -> list[str]:
    failures: list[str] = []
    run_fs(
        """
        login
        root
        123456
        create stats_note
        open stats_note w
        write 0 hello
        close 0
        open stats_note r
        read 0 5
        close 0
        logout
        exit
        """,
        tmp,
    )

    io_path = tmp / "debug_memory/users/0/io_stats.json"
    kfs_path = tmp / "debug_memory/users/0/kfs_stats.json"
    try:
        io_stats = json.loads(io_path.read_text(encoding="utf-8"))
    except Exception as exc:
        failures.append(f"agent_stats:io_read:{exc}")
        io_stats = {}
    try:
        kfs_stats = json.loads(kfs_path.read_text(encoding="utf-8"))
    except Exception as exc:
        failures.append(f"agent_stats:kfs_read:{exc}")
        kfs_stats = {}

    if not io_stats.get("files"):
        failures.append("agent_stats:io_files")
    if not kfs_stats.get("hot_files"):
        failures.append("agent_stats:kfs_hot_files")

    restored = run_fs(
        """
        login
        root
        123456
        init_io_opt
        io_stats
        logout
        exit
        """,
        tmp,
    )
    if "Inode" not in restored:
        failures.append("agent_stats:io_restore")

    return failures


def validate_kfs_materialization_flow(tmp: Path) -> list[str]:
    failures: list[str] = []
    run_fs(
        """
        login
        root
        123456
        create hotnote
        open hotnote w
        write 0 hello
        close 0
        open hotnote r
        read 0 5
        close 0
        logout
        exit
        """,
        tmp,
    )

    for cmd in (
        ["python", "-m", "ai.cli", "set_user", "0"],
        ["python", "-m", "ai.cli", "analyze"],
    ):
        result = subprocess.run(
            cmd,
            cwd=tmp,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            failures.append(f"kfs_materialize:returncode:{' '.join(cmd)}")

    try:
        learned = json.loads(
            (tmp / "debug_memory/users/0/agent/memory/long_term/learned_params.json").read_text(encoding="utf-8")
        )
    except Exception as exc:
        return failures + [f"kfs_materialize:learned_read:{exc}"]

    hot_files = learned.get("parameters", {}).get("hot_files", [])
    if not hot_files:
        failures.append("kfs_materialize:learned_hot_files")

    text = run_fs(
        """
        login
        root
        123456
        kfs_ai_select
        open hotnote r
        read 0 5
        close 0
        kfs_memory_map
        logout
        exit
        """,
        tmp,
    )

    checks = {
        "stored": "File hotnote stored in KFS" in text,
        "fast_path": "from KFS (fast path)" in text,
        "content": "Content: hello" in text,
        "memory_map": "hotnote (in-KFS)" in text,
    }
    failures.extend(f"kfs_materialize:{name}" for name, ok in checks.items() if not ok)
    return failures


def validate_io_agent_prefetch_flow(tmp: Path) -> list[str]:
    failures: list[str] = []
    stats_dir = tmp / "debug_memory/users/0"
    stats_dir.mkdir(parents=True, exist_ok=True)
    (tmp / "debug_memory/current_user.json").write_text('{"uid": 0}', encoding="utf-8")
    (stats_dir / "io_stats.json").write_text(
        json.dumps(
            {
                "cache_hits": 0,
                "cache_misses": 8,
                "total_prefetched_blocks": 8,
                "file_count": 1,
                "sequential_files": 1,
                "random_files": 0,
                "average_prefetch_window": 3,
                "total_read_operations": 8,
                "files": [
                    {
                        "ino": 321,
                        "total_reads": 8,
                        "type": "sequential",
                        "sequential_transitions": 6,
                        "random_transitions": 0,
                        "transition_count": 6,
                        "current_prefetch_window": 3,
                        "last_block": 7,
                    }
                ],
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    for cmd in (
        ["python", "-m", "ai.cli", "set_user", "0"],
        ["python", "-m", "ai.cli", "analyze"],
    ):
        result = subprocess.run(
            cmd,
            cwd=tmp,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            failures.append(f"io_prefetch:returncode:{' '.join(cmd)}")

    try:
        learned = json.loads(
            (tmp / "debug_memory/users/0/agent/memory/long_term/learned_params.json").read_text(encoding="utf-8")
        )
    except Exception as exc:
        return failures + [f"io_prefetch:learned_read:{exc}"]

    windows = learned.get("parameters", {}).get("file_prefetch_windows", {})
    if int(windows.get("321", 0)) < 6:
        failures.append("io_prefetch:sequential_window")

    restored = run_fs(
        """
        login
        root
        123456
        init_io_opt
        io_stats
        logout
        exit
        """,
        tmp,
    )
    if "Inode 321" not in restored or not any(f"预取窗口: {window}" in restored for window in range(6, 11)):
        failures.append("io_prefetch:c_applied_window")

    return failures


def validate_security_agent_flow(tmp: Path) -> list[str]:
    failures: list[str] = []
    commands = [["python", "-m", "ai.cli", "set_user", "0"]]
    commands.extend(["python", "-m", "ai.cli", "record", "delete", f"old_{idx}.txt"] for idx in range(5))
    commands.extend(["python", "-m", "ai.cli", "record", "create", f"new_{idx}.txt"] for idx in range(8))
    commands.append(["python", "-m", "ai.cli", "analyze"])

    for cmd in commands:
        result = subprocess.run(
            cmd,
            cwd=tmp,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            failures.append(f"security_agent:returncode:{' '.join(cmd)}")

    try:
        learned = json.loads(
            (tmp / "debug_memory/users/0/agent/memory/long_term/learned_params.json").read_text(
                encoding="utf-8"
            )
        )
    except Exception as exc:
        return failures + [f"security_agent:learned_read:{exc}"]

    params = learned.get("parameters", {})
    delete_threshold = int(params.get("delete_threshold", 0))
    modify_threshold = int(params.get("modify_threshold", 0))
    if delete_threshold != 5:
        failures.append("security_agent:delete_threshold")
    if modify_threshold != 8:
        failures.append("security_agent:modify_threshold")

    text = run_fs(
        """
        login
        root
        123456
        user_profile
        logout
        exit
        """,
        tmp,
    )
    expected = f"Security thresholds: delete={delete_threshold} modify={modify_threshold}"
    if expected not in text:
        failures.append("security_agent:c_applied_thresholds")

    return failures


def validate_gui_bridge(tmp: Path) -> list[str]:
    old_cwd = Path.cwd()
    sys.path.insert(0, str(ROOT))
    try:
        os.chdir(tmp)
        from gui.c_integration import CSystemClient

        client = CSystemClient()
        failures: list[str] = []
        if not client.start_system():
            return ["gui_bridge:start"]

        try:
            login_output = client.login("root", "123456")
            if "Login successful" not in login_output or not client.is_logged_in:
                failures.append("gui_bridge:login")

            dir_output = client.list_dir()
            if "Directory contents:" not in dir_output:
                failures.append("gui_bridge:list_dir")

            blocks = client.get_block_status()
            if len(blocks) != 512:
                failures.append("gui_bridge:blocks_length")
        finally:
            client.stop()
        return failures
    finally:
        os.chdir(old_cwd)


def main() -> int:
    failures = compile_python()

    build = build_filesystem()
    if build.returncode != 0:
        print("BUILD FAILED")
        print(build.stdout)
        print(build.stderr)
        return 1

    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp = Path(tmp_dir)
        prepare_env(tmp)
        failures.extend(validate_core_flow(tmp))
        failures.extend(validate_link_flow(tmp))
        failures.extend(validate_copy_move_flow(tmp))
        failures.extend(validate_cli_flow(tmp))
        failures.extend(validate_user_memory_isolation(tmp))
        failures.extend(validate_agent_stats_persistence(tmp))
        failures.extend(validate_kfs_materialization_flow(tmp))
        failures.extend(validate_io_agent_prefetch_flow(tmp))
        failures.extend(validate_security_agent_flow(tmp))
        failures.extend(validate_gui_bridge(tmp))

    if failures:
        print("VALIDATION FAILED")
        for item in failures:
            print(item)
        return 1

    print("VALIDATION PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
