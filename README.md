# Simple shared object generator

Writing your own linker from scratch is a daunting endeavor. This repository aims to help you figure out the basics of 64-bit [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) files generated by Unix its [ld](https://en.wikipedia.org/wiki/Linker_(computing)#Common_implementations) linker.

This repository contains three tiny C programs:

1. `generate_simple_o.c`, which generates `simple.o`
2. `generate_simple_so.c`, which generates `simple.so`
3. `generate_full_so.c`, which generates `full.so`

The two `simple` programs generate a `.o` and `.so` based off of `simple.s`, which exports an `a` string containing the text `a^`:

```nasm
global a ; Exports the symbol "a"

section .data ; The default section is ".text", which is for code, not data

; Defines the symbol "a" containing the text "a^", with a null-terminator added
; (db stands for Define Byte)
a: db "a^", 0
```

The `full` program generates a `.so` based off of `full.s`, which exports several strings and functions.

## Running

`simple.s` and `full.s` not only serve as documentation, but also as tests. The "Verifying correctness" headers found below use [nasm](https://en.wikipedia.org/wiki/Netwide_Assembler) to compile the `.s` into a `.o`, and ld in order to link that into a `.so`. If the command prints anything, it always means our program failed the test. Any printed output shows which bytes were wrong.

After you've played with the below commands, I recommend commenting out the first line of `simple.s` that exports the string, to see whether you can fix `generate_simple_o.c` so that it passes its test again. Once you've got that working, you can try the same on `full.s` with `generate_full_so.c`. After that, you can move on to testing more significant changes to `full.s`.

### generate_simple_o.c

1. `gcc generate_simple_o.c && ./a.out` (or `nasm -f elf64 simple.s`)
2. `ld -shared --hash-style=sysv simple.o -o simple.so`
3. `gcc run_simple.c && ./a.out`, which should print `a^`, coming from simple.o

This is the ELF layout of the generated `simple.o`:

```
0x000: ELF header
0x040: Section header table
0x180: Data (.data)
0x190: Section names (.shstrtab)
0x1c0: Symbol info (.symtab)
0x220: Symbol names (.strtab)
```

#### Verifying correctness

```bash
gcc -Wall -Wextra -Wpedantic -Werror -Wfatal-errors -g generate_simple_o.c && ./a.out && xxd simple.o > mine.hex && \
nasm -f elf64 simple.s && xxd simple.o > goal.hex && \
diff mine.hex goal.hex
```

### generate_simple_so.c

1. `gcc generate_simple_so.c && ./a.out`
2. `gcc run_simple.c && ./a.out`, which should print `a^`, coming from simple.so

This is the ELF layout of the generated `simple.so`:

```
0x0000: ELF header
0x0040: Program headers
0x0120: Hash (.hash)
0x0120: Dynamic symbols (.dynsym)
0x0168: Dynamic strings (.dynstr)
0x0168: Dynamic info (.dynamic)
0x2000: Data (.data)
0x2008: Symbol info (.symtab)
0x2080: Symbol names (.strtab)
0x2094: Section names (.shstrtab)
0x20e0: Section header table
```

#### Verifying correctness

```bash
gcc -Wall -Wextra -Wpedantic -Werror -Wfatal-errors -g generate_simple_so.c && ./a.out && xxd simple.so > mine.hex && \
nasm -f elf64 simple.s && ld -shared --hash-style=sysv simple.o -o simple.so && xxd simple.so > goal.hex && \
diff mine.hex goal.hex
```

### generate_full_so.c

1. `gcc generate_full_so.c && ./a.out`
2. `gcc run_full.c && ./a.out`, which should print `a^`, `42`, `1337`, and `69`, coming from full.so

This is the ELF layout of the generated `full.so`:

```
0x0000: ELF header
0x0040: Program headers
0x0190: Hash (.hash)
0x01d0: Dynamic symbols (.dynsym)
0x02d8: Dynamic strings (.dynstr)
0x1000: Machine code (.text)
0x2f50: Dynamic info (.dynamic)
0x3000: Data (.data)
0x3018: Symbol info (.symtab)
0x3168: Symbol names (.strtab)
0x3193: Section names (.shstrtab)
0x31e8: Section header table
```

#### Verifying correctness

```bash
gcc -Wall -Wextra -Wpedantic -Werror -Wfatal-errors -g generate_full_so.c && ./a.out && xxd full.so > mine.hex && \
nasm -f elf64 full.s && ld -shared --hash-style=sysv full.o -o full.so && xxd full.so > goal.hex && \
diff mine.hex goal.hex
```
