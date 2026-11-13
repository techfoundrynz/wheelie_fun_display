"""Audit the generated SDK settings after building with PlatformIO."""

from pathlib import Path
import sys

if "--all" in sys.argv:
    import subprocess
    from project_config import ROOT, environments

    for environment in environments():
        subprocess.run(
            [sys.executable, __file__, str(ROOT / ("sdkconfig." + environment))],
            check=True,
        )
    raise SystemExit(0)
path = Path(sys.argv[1] if len(sys.argv) > 1 else "sdkconfig.waveshare_esp32s3_206")
values = dict(
    line.split("=", 1)
    for line in path.read_text(encoding="utf-8").splitlines()
    if line.startswith("CONFIG_") and "=" in line
)
expected = {
    "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE": "y",
    "CONFIG_PARTITION_TABLE_CUSTOM": "y",
    "CONFIG_ESP_MAIN_TASK_STACK_SIZE": "32768",
    "CONFIG_ESPTOOLPY_FLASHMODE_QIO": "y",
    "CONFIG_ESPTOOLPY_FLASHFREQ_80M": "y",
    "CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL": "512",
    "CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL": "32768",
    "CONFIG_COMPILER_OPTIMIZATION_PERF": "y",
    "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE": "y",
    "CONFIG_SPIRAM_MEMTEST": "y",
    "CONFIG_ESP_INT_WDT": "y",
    "CONFIG_ESP_TASK_WDT_EN": "y",
    "CONFIG_BT_NIMBLE_ENABLED": "y",
    "CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU": "247",
    "CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL": "y",
    "CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC": "y",
    "CONFIG_BT_NIMBLE_ROLE_CENTRAL": "y",
    "CONFIG_BT_NIMBLE_ROLE_PERIPHERAL": "y",
    "CONFIG_BT_NIMBLE_ROLE_BROADCASTER": "y",
    "CONFIG_BT_NIMBLE_MAX_CONNECTIONS": "2",
    "CONFIG_BT_NIMBLE_NVS_PERSIST": "y",
    "CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM": "y",
    "CONFIG_BT_NIMBLE_MAX_BONDS": "4",
    "CONFIG_BT_NIMBLE_MAX_CCCDS": "8",
}
target = values.get("CONFIG_IDF_TARGET", "").strip('"')
if target == "esp32s3":
    expected.update(
        {
            "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ": "240",
            "CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL": "0",
            "CONFIG_SPIRAM_MODE_OCT": "y",
            "CONFIG_SPIRAM_SPEED_80M": "y",
            "CONFIG_SPIRAM_FETCH_INSTRUCTIONS": "y",
            "CONFIG_SPIRAM_XIP_FROM_PSRAM": "y",
            "CONFIG_SPIRAM_RODATA": "y",
            "CONFIG_ESP32S3_INSTRUCTION_CACHE_SIZE": "0x8000",
            "CONFIG_ESP32S3_INSTRUCTION_CACHE_LINE_SIZE": "32",
            "CONFIG_ESP32S3_DATA_CACHE_SIZE": "0x10000",
            "CONFIG_ESP32S3_DATA_CACHE_LINE_SIZE": "64",
            "CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP": "y",
            "CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM": "4",
            "CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM": "8",
            "CONFIG_ESP_WIFI_CACHE_TX_BUFFER_NUM": "16",
            "CONFIG_ESP_WIFI_RX_BA_WIN": "4",
        }
    )
elif target == "esp32p4":
    expected.update(
        {
            "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ": "400",
            "CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL": "0",
            "CONFIG_ESP32P4_REV_MIN_FULL": "301",
            "CONFIG_BOOTLOADER_LOG_LEVEL": "2",
            "CONFIG_SPIRAM_SPEED_250M": "y",
            "CONFIG_SPIRAM_SPEED": "250",
            "CONFIG_CACHE_L2_CACHE_256KB": "y",
            "CONFIG_CACHE_L2_CACHE_LINE_SIZE": "128",
            "CONFIG_SPIRAM_XIP_FROM_PSRAM": "y",
            "CONFIG_SPIRAM_FETCH_INSTRUCTIONS": "y",
            "CONFIG_SPIRAM_RODATA": "y",
            "CONFIG_SPIRAM_PRE_CONFIGURE_MEMORY_PROTECTION": "y",
            "CONFIG_BT_CONTROLLER_DISABLED": "y",
            "CONFIG_ESP_WIFI_REMOTE_ENABLED": "y",
            "CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED": "y",
            "CONFIG_SLAVE_IDF_TARGET_ESP32C6": "y",
            "CONFIG_ESP_HOSTED_HOST_TRANSPORT_BUS_SDIO": "y",
            "CONFIG_ESP_HOSTED_SDIO_SLOT_1": "y",
            "CONFIG_ESP_HOSTED_HOST_SDIO_BUS_WIDTH_4": "y",
            "CONFIG_ESP_HOSTED_HOST_SDIO_RX_STREAMING_MODE": "y",
            "CONFIG_ESP_HOSTED_HOST_SDIO_USE_MEMPOOL": "y",
            "CONFIG_ESP_HOSTED_HOST_SDIO_CLK_KHZ": "25000",
            "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_CLK": "18",
            "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_CMD": "19",
            "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D0": "14",
            "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D1": "15",
            "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D2": "16",
            "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D3": "17",
            "CONFIG_ESP_HOSTED_HOST_RESET_GPIO": "54",
            "CONFIG_ESP_HOSTED_HOST": "y",
            "CONFIG_ESP_HOSTED_HOST_FEAT_BT": "y",
            "CONFIG_ESP_HOSTED_HOST_FEAT_WIFI": "y",
            "CONFIG_ESP_HOSTED_HOST_FEAT_RPC_EXT_V2_READY": "y",
        }
    )
    if values.get("CONFIG_ESP32P4_SELECTS_REV_LESS_V3") == "y":
        raise SystemExit("P4 profile must not select pre-v3 silicon")
else:
    raise SystemExit("Unsupported SDK target: " + target)

from project_config import ROOT, option

environment = path.name.removeprefix("sdkconfig.")
flash_size = option(environment, "board_upload.flash_size")
expected["CONFIG_ESPTOOLPY_FLASHSIZE_" + flash_size] = "y"
errors = [
    f"{name}: expected {value}, got {values.get(name, 'unset')}"
    for name, value in expected.items()
    if values.get(name) != value
]
if target == "esp32p4":
    import json

    bootloader = json.loads(
        (
            ROOT / ".pio/build" / environment / "bootloader/config/sdkconfig.json"
        ).read_text(encoding="utf-8")
    )
    for name in ("ESP_REV_MIN_FULL", "ESP_REV_MAX_FULL", "BOOTLOADER_LOG_LEVEL"):
        if str(bootloader.get(name)) != values.get("CONFIG_" + name):
            errors.append("Bootloader SDK configuration is stale: " + name)
if errors:
    raise SystemExit("\n".join(errors))
print(f"SDK audit passed: {len(expected)} settings, code/RODATA in PSRAM")
