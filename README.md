# Simple shared object generator

Writing your own linker from scratch is a daunting endeavor. This repository aims to help you figure out the basics of 64-bit [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) files generated by Unix its [ld](https://en.wikipedia.org/wiki/Linker_(computing)#Common_implementations) linker.

There are three tiny C programs:

1. `generate_simple_o.c` generates `simple.o`
2. `generate_simple_so.c` generates `simple.so`
3. `generate_full_so.c` generates `full.so`

The two `simple` programs generate a `.o` and `.so` based off of `simple.s`, which exports a global `a` string containing the text `a^`:

```nasm
global a

section .data

a: db "a^", 0
```

The `full` program generates a `.so` based off of `full.s`, which exports several global strings and functions.

## Running

`simple.s` and `full.s` not only serve as documentation, but also as tests. The "Verifying correctness" headers found below use [nasm](https://en.wikipedia.org/wiki/Netwide_Assembler) to compile the `.s` into a `.o`, and `ld` in order to link that into a `.so`. If our program's result differs from nasm+ld's result, it always means the test failed, and our program needs to be edited.

If you're trying to learn the ELF format, I recommend slightly modifying `full.s`, and seeing whether you can fix `generate_full_so.c` in order to have the test still pass.

### generate_simple_o.c

1. `gcc generate_simple_o.c && ./a.out` (or `nasm -f elf64 simple.s`)
2. `ld -shared --hash-style=sysv simple.o -o simple.so`
3. `gcc run_simple.c && ./a.out`, which should print `a^`, coming from simple.o

#### Verifying correctness

1. `gcc -Wall -Wextra -Wpedantic -Werror -Wfatal-errors -g generate_simple_o.c && ./a.out && xxd simple.o > mine.hex`
2. `nasm -f elf64 simple.s && xxd simple.o > goal.hex`
3. `diff mine.hex goal.hex` shows no output if the generated `simple.o` is correct

### generate_simple_so.c

1. `gcc generate_simple_so.c && ./a.out`
2. `gcc run_simple.c && ./a.out`, which should print `a^`, coming from simple.so

#### Verifying correctness

1. `gcc -Wall -Wextra -Wpedantic -Werror -Wfatal-errors -g generate_simple_so.c && ./a.out && xxd simple.so > mine.hex`
2. `nasm -f elf64 simple.s && ld -shared --hash-style=sysv simple.o -o simple.so && xxd simple.so > goal.hex`
3. `diff mine.hex goal.hex` shows no output if the generated `simple.so` is correct

### generate_full_so.c

1. `gcc generate_full_so.c && ./a.out`
2. `gcc run_full.c && ./a.out`, which should print `a^` and `42`, coming from full.so

#### Verifying correctness

1. `gcc -Wall -Wextra -Wpedantic -Werror -Wfatal-errors -g generate_full_so.c && ./a.out && xxd full.so > mine.hex`
2. `nasm -f elf64 full.s && ld -shared --hash-style=sysv full.o -o full.so && xxd full.so > goal.hex`
3. `diff mine.hex goal.hex` shows no output if the generated `full.so` is correct
