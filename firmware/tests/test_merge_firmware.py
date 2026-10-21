"""Check merge commands for old script and new executable esptool packages."""

from pathlib import Path
import runpy
import sys
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "merge_firmware.py"


class BuildEnv:
    def __init__(self, tool):
        self.values = {
            "$OBJCOPY": tool,
            "$PYTHONEXE": sys.executable,
            "$BUILD_DIR": "/build directory/x4",
            "$BOOT": "/build directory/bootloader.bin",
            "$PART": "/build directory/partitions.bin",
        }
        self.options = {
            "board_build.flash_mode": "dio",
            "board_build.f_flash": "80000000L",
            "board_upload.flash_size": "16MB",
        }

    def BoardConfig(self):
        return {"build.mcu": "esp32c3"}

    def GetProjectOption(self, key, default):
        return self.options.get(key, default)

    def subst(self, value):
        return self.values.get(value, value)

    def get(self, key, default):
        return [("0x0", "$BOOT"), ("0x8000", "$PART")] if key == "FLASH_EXTRA_IMAGES" else default

    def AddPostAction(self, target, callback):
        self.callback = callback


class MergeFirmwareTests(unittest.TestCase):
    def run_merge(self, tool, result=0):
        env = BuildEnv(tool)
        module = runpy.run_path(str(SCRIPT), init_globals={"Import": lambda _: None, "env": env})
        with patch("subprocess.call", return_value=result) as execute, patch("builtins.print"):
            status = module["merge_bin"]([], ["/build directory/firmware.bin"], env)
        return execute.call_args.args[0], status

    def test_native_tool_keeps_paths_with_spaces_and_flash_settings(self):
        args, status = self.run_merge('"/tool directory/esptool"')
        self.assertEqual(status, 0)
        self.assertEqual(args[:3], ["/tool directory/esptool", "--chip", "esp32c3"])
        self.assertEqual(args[args.index("--flash_freq") + 1], "80m")
        self.assertEqual(args[args.index("--flash_size") + 1], "16MB")
        self.assertEqual(args[args.index("-o") + 1], "/build directory/x4/firmware_merged.bin")
        self.assertEqual(args[-6:], ["0x0", "/build directory/bootloader.bin", "0x8000", "/build directory/partitions.bin", "0x10000", "/build directory/firmware.bin"])

    def test_legacy_script_uses_configured_python(self):
        args, _ = self.run_merge("/tool directory/esptool.py")
        self.assertEqual(args[:2], [sys.executable, "/tool directory/esptool.py"])

    def test_merge_failure_is_returned_to_build(self):
        _, status = self.run_merge("/tool/esptool", result=17)
        self.assertEqual(status, 17)


if __name__ == "__main__":
    unittest.main()
