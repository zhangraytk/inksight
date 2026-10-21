Import("env")
import os
import subprocess

def merge_bin(source, target, env):
    firmware = str(target[0])
    board = env.BoardConfig()
    mcu = board.get("build.mcu", "esp32")
    flash_mode = env.GetProjectOption("board_build.flash_mode", board.get("build.flash_mode", "dio"))
    flash_freq = env.GetProjectOption("board_build.f_flash", board.get("build.f_flash", "40000000L")).replace("L", "")
    flash_freq_m = str(int(int(flash_freq) / 1000000)) + "m"
    flash_size = env.GetProjectOption("board_upload.flash_size", board.get("upload.flash_size", "4MB"))

    # Use the same tool as ELF-to-BIN: newer platforms ship a native esptool
    # executable, while older platforms configure an esptool.py script.
    esptool = env.subst("$OBJCOPY").strip('"')
    output = os.path.join(env.subst("$BUILD_DIR"), "firmware_merged.bin")

    extra = env.get("FLASH_EXTRA_IMAGES", [])
    print("=== FLASH_EXTRA_IMAGES ===")
    for offset, path in extra:
        print(f"  {offset} -> {env.subst(path)}")
    print(f"  APP -> 0x10000 {firmware}")
    print("==========================")

    cmd = [
        esptool,
        "--chip", mcu,
        "merge_bin",
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq_m,
        "--flash_size", flash_size,
        "-o", output,
    ]

    for offset, path in extra:
        cmd.extend([offset, env.subst(path)])

    cmd.extend(["0x10000", firmware])

    if esptool.lower().endswith(".py"):
        cmd.insert(0, env.subst("$PYTHONEXE").strip('"'))
    return subprocess.call(cmd)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
