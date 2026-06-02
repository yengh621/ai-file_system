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


def validate_nlp_flow(tmp: Path) -> list[str]:
    text = run_fs(
        """
        login
        root
        123456
        nlp create auto.txt
        dir
        delete auto.txt
        logout
        exit
        """,
        tmp,
    )
    checks = {
        "nlp_exec": "正在执行: create auto.txt" in text,
        "nlp_create": "Create successful." in text,
        "nlp_visible": "auto.txt" in text,
    }
    return [f"nlp_flow:{name}" for name, ok in checks.items() if not ok]


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
        failures.extend(validate_nlp_flow(tmp))
        failures.extend(validate_cli_flow(tmp))
        failures.extend(validate_user_memory_isolation(tmp))
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
