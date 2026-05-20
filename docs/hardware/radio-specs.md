# Radio Hardware Specifications for Edge16

Edge16 supports only RadioMaster TX16S MK2 and RadioMaster TX16S MK3.

| Radio | Build flavor | MCU family | Display | Firmware artifact | Notes |
|---|---|---|---|---|---|
| RadioMaster TX16S MK2 | `tx16s` | STM32F4 | 480×320 color touch LCD | `tx16s-*.bin` | Classic TX16S color target. |
| RadioMaster TX16S MK3 | `tx16smk3` | STM32H7 | 480×320 color touch LCD | `tx16smk3-*.uf2` | Different update path from MK2; use MK3 artifact only. |

> [!WARNING]
> Do not flash Edge16 firmware on other EdgeTX radios. Release automation intentionally publishes only TX16S MK2/MK3 artifacts.

For deeper upstream hardware background, see EdgeTX hardware documentation, but treat any non-TX16S target information as upstream reference only, not Edge16 support.
