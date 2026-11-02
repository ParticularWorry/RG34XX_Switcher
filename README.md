# RG34XX-H Refresh Switcher

The RG34XX-H running **MustardOS Andromeda 2606.0** may output approximately **107 Hz instead of 60 Hz**, causing microstutter in 60 FPS games. The problem appears to stem from the Allwinner display driver's clock configuration.

This on-device tool applies the **60 Hz LCD timings discovered by TheGammaSqueeze** to the existing boot package, without replacing unrelated board settings or controller mappings. It also supports restoring the original timings.

## Credits

**Full credit for discovering and publishing the original RG34XX 60 Hz fix goes to [TheGammaSqueeze](https://github.com/TheGammaSqueeze/rgxx_boot_package_mods/tree/rg34xx).**

This project is simply a targeted way to apply that fix on Andromeda; it does not claim credit for the timing solution.

## Compatibility and warning

- Tested successfully **on my own RG34XX-H running Andromeda 2606.0**. Your results may differ.
- **Use at your own risk.** This tool writes to the boot area. There is **no guarantee** against an unbootable SD card, device malfunction, or other damage. Back up your SD card before use.
- Other models and derivatives, including the **RG34XX-SP**, are **not supported or verified**. They may require changes to the code, offsets, checksums, and device-specific checks. Do not use this build on them as-is.

## Background

[MustardOS bug report: RG34XX-H incorrect display refresh rate (~107 Hz) in Andromeda 2606.0](https://community.muos.dev/t/rg34xx-h-incorrect-display-refresh-rate-107-hz-in-andromeda-2606-0/1825)

Disclaimer: This tool is 100% AI generated.
