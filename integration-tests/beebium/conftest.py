from __future__ import annotations

import contextlib
import datetime as _dt
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

_HERE = Path(__file__).resolve()
_NIO_CONFIG_ROOT = _HERE.parents[2]
_WORKSPACE = _NIO_CONFIG_ROOT.parents[1]
_FN_ROM_TESTS = _WORKSPACE / "repos" / "fn-rom" / "integration-tests" / "beebium"

if str(_FN_ROM_TESTS) not in sys.path:
    sys.path.insert(0, str(_FN_ROM_TESTS))

from beebium_test_env import add_fujinet_tools_to_path, ensure_environment  # noqa: E402
from evidence import ScreenEvidenceRecorder  # noqa: E402
from fujinet_runner import IsolatedFujinet  # noqa: E402

ensure_environment()
add_fujinet_tools_to_path()


def pytest_addoption(parser):
    group = parser.getgroup("nio-config-beebium", "nio-config Beebium tests")
    group.addoption(
        "--fn-rom",
        action="store",
        default=os.environ.get("FN_ROM", str(_WORKSPACE / "repos" / "fn-rom" / "build" / "fujinet.rom")),
        help="BBC fn-rom image to load",
    )
    group.addoption(
        "--fn-rom-slot",
        action="store",
        type=int,
        default=int(os.environ.get("FN_ROM_SLOT", "12")),
        help="sideways slot for fn-rom",
    )
    group.addoption(
        "--confnio-bbc-ssd",
        action="store",
        default=os.environ.get("CONFNIO_BBC_SSD", str(_WORKSPACE / "build" / "images" / "confnio-bbc.ssd")),
        help="standalone BBC CONFNIO SSD image built by the runner",
    )
    group.addoption(
        "--fujinet-bin",
        action="store",
        default=os.environ.get("FUJINET_BIN", ""),
        help="real posix fujinet-nio PTY binary",
    )
    group.addoption(
        "--screen-evidence-dir",
        action="store",
        default=os.environ.get("NIO_CONFIG_BEEBIUM_EVIDENCE_ROOT", ""),
        help="directory for Beebium screen evidence",
    )
    group.addoption(
        "--no-screen-evidence",
        action="store_true",
        default=os.environ.get("NIO_CONFIG_BEEBIUM_NO_EVIDENCE", "") in ("1", "true", "yes"),
        help="disable Beebium screen evidence capture",
    )


def pytest_configure(config):
    if not config.getoption("--no-screen-evidence"):
        requested = config.getoption("--screen-evidence-dir")
        if requested:
            evidence_root = Path(requested).expanduser()
        else:
            stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
            evidence_root = _NIO_CONFIG_ROOT / "test-evidence" / f"beebium-{stamp}"
        config._nio_config_beebium_evidence_root = evidence_root


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    setattr(item, "rep_" + call.when, outcome.get_result())


def pytest_report_header(config):
    root = getattr(config, "_nio_config_beebium_evidence_root", None)
    if root is None:
        return "nio-config Beebium screen evidence: disabled"
    return f"nio-config Beebium screen evidence: {root}"


@pytest.fixture()
def screen_evidence(pytestconfig, request):
    root = getattr(pytestconfig, "_nio_config_beebium_evidence_root", None)
    if root is None:
        yield None
        return

    recorder = ScreenEvidenceRecorder(
        root=Path(root),
        lane="bbc-config-nio",
        nodeid=request.node.nodeid,
    )
    try:
        yield recorder
    finally:
        report = getattr(request.node, "rep_call", None)
        status = report.outcome if report is not None else "unknown"
        recorder.finish(status=status)


@pytest.fixture(scope="session")
def beebium_paths(pytestconfig):
    fn_rom = Path(pytestconfig.getoption("--fn-rom")).expanduser()

    if not fn_rom.is_file():
        pytest.skip(f"fn-rom image not found at {fn_rom}")

    return {
        "server": Path(os.environ["BEEBIUM_SERVER"]),
        "mos": Path(os.environ["BEEBIUM_MOS"]),
        "basic": Path(os.environ["BEEBIUM_BASIC"]) if os.environ.get("BEEBIUM_BASIC") else None,
        "fn_rom": fn_rom,
        "fn_slot": int(pytestconfig.getoption("--fn-rom-slot")),
    }


@pytest.fixture()
def real_fujinet_config_nio(pytestconfig):
    binary_opt = pytestconfig.getoption("--fujinet-bin")
    if not binary_opt:
        pytest.skip("FUJINET_BIN is not set")

    binary = Path(binary_opt).expanduser()
    if binary.name == "run-fujinet-nio" and (binary.parent / "fujinet-nio").is_file():
        binary = binary.parent / "fujinet-nio"
    if not binary.is_file() or not os.access(binary, os.X_OK):
        pytest.skip(f"fujinet-nio PTY binary not found/executable at {binary}")

    fn = IsolatedFujinet(
        binary,
        extra_config=(
            "boot:\n"
            "  mode: config\n"
            "  config_uri: host:/boot/bbc/FN-BOOT.ssd\n"
            "  readonly: true\n"
        ),
    )
    data_root = fn.run_dir / "fujinet-data"
    boot_src = Path(os.environ["FUJINET_NIO_HOME"]) / "distfiles" / "boot"
    if not (boot_src / "bbc" / "FN-BOOT.ssd").is_file():
        pytest.skip(f"BBC FujiNet utility disk not found under {boot_src}")
    shutil.copytree(boot_src, data_root / "boot", dirs_exist_ok=True)

    appstore = data_root / "FujiNet" / "app-store" / "v1" / "config-nio"
    appstore.mkdir(parents=True, exist_ok=True)
    (appstore / "hosts.bin").write_bytes(b"sd0:/\nfujinet.diller.org\nfujinet.online\n")
    (appstore / "slot-011.bin").write_bytes(
        b"\x01\x00/192.168.1.101/bbc/blank.ssd"
    )

    source_dir = fn.run_dir / "ssd-source"
    source_dir.mkdir(parents=True, exist_ok=True)
    (source_dir / "HELLO").write_text("CONFIG NIO BEEBIUM TEST\r", encoding="ascii")

    image_dir = data_root / "cfg" / "images"
    image_dir.mkdir(parents=True, exist_ok=True)
    large_dir = data_root / "bbc"
    large_dir.mkdir(parents=True, exist_ok=True)
    for name in (
        "aardvark",
        "basic2.ssd",
        "blank.ssd",
        "bwc.ssd",
        "chuck.ssd",
        "ctests.ssd",
        "empty.ssd",
        "fcs.ssd",
        "fish",
        "fn-boot.ssd",
        "fs.ssd",
        "fstest.ssd",
        "impetus_mode7.ssd",
        "iss.ssd",
        "nellan.ssd",
        "net-nio-clib.ssd",
        "net-nio.ssd",
        "net.ssd",
        "openbas.ssd",
        "osargs.ssd",
        "osfile.ssd",
        "pent.ssd",
        "play.ssd",
        "play_bak.ssd",
        "weather.ssd",
        "this_is_a_very_long_file_name_that_should_scroll_over_the_window.ssd",
    ):
        if "." in name:
            (large_dir / name).write_bytes(b"")
        else:
            (large_dir / name).mkdir(exist_ok=True)
    create_ssd = _NIO_CONFIG_ROOT / "scripts" / "create_ssd.py"
    if not create_ssd.is_file():
        pytest.skip(f"SSD generator not found at {create_ssd}")
    missing = [tool for tool in ("basictool", "dfstool") if not shutil.which(tool)]
    if missing:
        pytest.skip(f"missing SSD tool(s): {', '.join(missing)}")
    subprocess.run(
        [
            "python3",
            str(create_ssd),
            "-i",
            str(source_dir),
            "-o",
            str(image_dir / "longer-navtest.ssd"),
            "-t",
            "NAVTEST",
        ],
        check=True,
        cwd=str(_NIO_CONFIG_ROOT),
    )

    fn.start()
    try:
        yield fn
    finally:
        keep = os.environ.get("NIO_CONFIG_BEEBIUM_KEEP_FUJINET", "") in ("1", "true", "yes")
        fn.cleanup(keep=keep)


@contextlib.contextmanager
def _launch_beebium(beebium_paths, real_fujinet):
    from beebium.client import Beebium

    extra_args = [
        "--sideways",
        f"{beebium_paths['fn_slot']}:rom:{beebium_paths['fn_rom']}",
        "--host-serial",
        f"mode=device:path={real_fujinet.pty_path}",
    ]

    with Beebium.launch(
        mos_filepath=str(beebium_paths["mos"]),
        basic_filepath=str(beebium_paths["basic"]) if beebium_paths["basic"] else None,
        server_filepath=str(beebium_paths["server"]),
        extra_args=extra_args,
    ) as bbc:
        if not bbc.system.wait_for_ready(timeout=5.0):
            raise RuntimeError("Beebium did not report READY within 5 seconds")
        bbc.system.set_speed_multiplier(0.0)
        yield bbc


@pytest.fixture()
def beebium_config_nio(beebium_paths, real_fujinet_config_nio, screen_evidence):
    with _launch_beebium(beebium_paths, real_fujinet_config_nio) as bbc:
        if screen_evidence is not None:
            setattr(bbc, "_fn_screen_evidence", screen_evidence)
        try:
            yield bbc
        finally:
            if screen_evidence is not None:
                screen_evidence.capture(bbc, "final")

