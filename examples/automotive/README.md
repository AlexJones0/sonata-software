# Instructions

**To run the automotive demo in CHERIoT mode:**
 - Navigate to the root of the `sonata-software` repository.
 - Make sure you are in a `nix develop` environment.
 - Run `xmake -P examples` to build all example firmware.
 - With Sonata mounted via USB, copy over the newly-built UF2 using e.g. 
 `cp build/cheriot/cheriot/release/automotive_demo.uf2 /media/yourname/SONATA`

**To run the automotive demo in legacy (non-CHERIoT) mode:**
 - TODO add instructions here
