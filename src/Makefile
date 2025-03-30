#   Dreamcast Back to Menu Makefile
#	Copyright (C) 2025 Ross Kilgariff
#
#	Based on Sylverant PSO Patcher Makefile
#	Copyright (C) 2011, 2013 Lawrence Sebald
#
#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License version 3 as
#   published by the Free Software Foundation.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.

AS = sh-elf-as -little
CC = sh-elf-gcc
OBJCOPY = sh-elf-objcopy -O binary
OBJS = startup.o go.o reloader.o
B2M_CFLAGS = -ml -ffunction-sections -fdata-sections -ffreestanding -Wall -Werror -std=c99 -Wextra -Os -flto=auto

all: clean disc.cdi

%.o: %.c
	sh-elf-gcc $(B2M_CFLAGS) $(CPPFLAGS) -c $< -o $@

reloader.elf: $(OBJS)
	$(CC) $(B2M_CFLAGS) -o $@ -Wl,--gc-sections -Tshlelf.xc -nostartfiles -nostdlib $^

reloader.bin: reloader.elf
	$(OBJCOPY) $^ $@

loader.elf: loader.s reloader.bin
	sh-elf-gcc -ml -o $@ loader.s -nostartfiles -nostdlib -Ttext=0x8c010000 -Os

loader.bin: loader.elf
	$(OBJCOPY) $^ $@ -R .stack 

disc.cdi: loader.elf main.bin
	mkdcdisc -e loader.elf -f main.bin -o disc.cdi

.PHONY : clean
clean:
	-rm -f reloader.bin reloader.elf loader.bin loader.elf $(OBJS) disc.cdi

.PHONY : rm-elf
rm-elf:
	-rm -f reloader.elf loader.elf
