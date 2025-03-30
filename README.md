## DC Reloader

### What is it?
A simple loader for Dreamcast indie games made with KallistiOS that lets them easily reload or return to a main menu when they call `arch_exit()`. Without this loader, games would have to manually reload the binary and call `arch_exec()`.

This was made for [Dream Disc '24](https://orcface.com/events/01-dream-disc-24) where I didn't actually realise that calling `arch_exit()` wouldn't be sufficient, so without this loader we would need to get 10 people to re-submit their games with the addition of manual binary loading and `arch_exec()` just to return to the menu.

### How does it work?
It consists of a simple loader based on the code from the Sylverant PSO Patcher and using a similar mechanism to [dc-load-ip](https://github.com/sizious/dcload-ip).

On load, this program copies itself to `0x8C004000` (AKA. `0xAC004000` in P2) which is unused space in the Dreamcast's memory that lies between the syscalls and IP.BIN. This means it stays in memory regardless of which game is running. It also writes a special value `0xdeadbeef` to the address `0xAC004004`, which KallistiOS reads to determine where to go on exit.

### Requirements
The KallistiOS gcc-sh4 toolchain is required (tested on version 14.2.0).
`mkdcdisc` should be available in the user's `PATH` in order to build the .cdi image.

### How do I use it?
Just put your `menu.bin` file in the `src` directory, change to that directory and run `make`. A CDI file called `disc.cdi` will be produced. After your menu boots, any subsequent times a program exits using `arch_exit()`, the menu will be loaded again.

### Attribution

Based on code from [Sylverant PSO Patcher](https://github.com/sega-dreamcast/sylverant-pso-patcher) by Lawrence Sebald, which includes code from [KallistiOS](https://github.com/KallistiOS/KallistiOS) originally by Megan Potter.
