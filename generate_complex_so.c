#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BYTES 420420
#define MAX_BUCKETS 32771 // From https://sourceware.org/git/?p=binutils-gdb.git;a=blob;f=bfd/elflink.c;h=6db6a9c0b4702c66d73edba87294e2a59ffafcf5;hb=refs/heads/master#l6560
#define MAX_SYMBOLS 420420

enum d_type {
    DT_NULL = 0, // Marks the end of the _DYNAMIC array
    DT_HASH = 4, // The address of the symbol hash table. This table refers to the symbol table indicated by the DT_SYMTAB element
    DT_STRTAB = 5, // The address of the string table
    DT_SYMTAB = 6, // The address of the symbol table
    DT_STRSZ = 10, // The total size, in bytes, of the DT_STRTAB string table
    DT_SYMENT = 11, // The size, in bytes, of the DT_SYMTAB symbol entry
};

enum p_type {
    PT_LOAD = 1, // Loadable segment
    PT_DYNAMIC = 2, // Dynamic linking information
};

enum p_flags {
    PF_W = 2, // Writable segment
    PF_R = 4, // Readable segment
};

enum sh_type {
    SHT_PROGBITS = 0x1, // Program data
    SHT_SYMTAB = 0x2, // Symbol table
    SHT_STRTAB = 0x3, // String table
    SHT_HASH = 0x5, // Symbol hash table
    SHT_DYNAMIC = 0x6, // Dynamic linking information
    SHT_DYNSYM = 0xb, // Dynamic linker symbol table
};

enum sh_flags {
    SHF_WRITE = 1, // Writable
    SHF_ALLOC = 2, // Occupies memory during execution
};

enum e_type {
    ET_DYN = 3, // Shared object
};

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

static u8 bytes[MAX_BYTES];
static size_t bytes_size;

static u32 chains[MAX_SYMBOLS];
static size_t chains_size;

static void push_byte(u8 byte) {
    if (bytes_size + 1 > MAX_BYTES) {
        fprintf(stderr, "error: MAX_BYTES of %d was exceeded\n", MAX_BYTES);
        exit(EXIT_FAILURE);
    }

    bytes[bytes_size++] = byte;
}

static void push_zeros(size_t count) {
    for (size_t i = 0; i < count; i++) {
        push_byte(0);
    }
}

static void push_string(char *str) {
    for (size_t i = 0; i < strlen(str); i++) {
        push_byte(str[i]);
    }
    push_byte('\0');
}

static void push_shstrtab() {
    push_byte(0);
    push_string(".symtab");
    push_string(".strtab");
    push_string(".shstrtab");
    push_string(".hash");
    push_string(".dynsym");
    push_string(".dynstr");
    push_string(".eh_frame");
    push_string(".dynamic");
    push_string(".data");
    push_zeros(2);
}

static void push_strtab() {
    push_byte(0);
    push_string("foo.s");
    push_string("_DYNAMIC");
    push_string("foo");
}

static void push_number(u64 n, size_t byte_count) {
    while (n > 0) {
        // Little-endian requires the least significant byte first
        push_byte(n & 0xff);
        byte_count--;

        n >>= 8; // Shift right by one byte
    }

    // Optional padding
    push_zeros(byte_count);
}

// See https://docs.oracle.com/cd/E19683-01/816-1386/chapter6-79797/index.html
// We specified the .symtab section to have an entry_size of 0x18 bytes
// TODO: I am pretty sure the third "size" argument here is actually "offset", seeing the spots we're calling this?
static void push_symbol(u32 name, u32 value, u32 size, u32 info, u32 other, u32 shndx) {
    push_number(name, 4);
    push_number(value, 4);
    push_number(size, 4);
    push_number(info, 4);
    push_number(other, 4);
    push_number(shndx, 4);
}

static void push_symtab() {
    // TODO: Some of these can be turned into enums using https://docs.oracle.com/cd/E19683-01/816-1386/chapter6-79797/index.html
    push_symbol(0, 0, 0, 0, 0, 0); // "<null>"
    push_symbol(1, 0xfff10004, 0, 0, 0, 0); // "foo.s"
    push_symbol(0, 0xfff10004, 0, 0, 0, 0); // "<null>"
    push_symbol(7, 0x50001, 0x1f50, 0, 0, 0); // "_DYNAMIC"
    push_symbol(16, 0x60010, 0x2000, 0, 0, 0); // "foo"
}

static void push_data() {
    push_string("bar");
    push_zeros(4);
}

// See https://docs.oracle.com/cd/E23824_01/html/819-0690/chapter6-42444.html
static void push_dynamic_entry(u64 tag, u64 value) {
    push_number(tag, 8);
    push_number(value, 8);
}

static void push_dynamic() {
    push_dynamic_entry(DT_HASH, 0x120);
    push_dynamic_entry(DT_STRTAB, 0x168);
    push_dynamic_entry(DT_SYMTAB, 0x138);
    push_dynamic_entry(DT_STRSZ, 5);
    push_dynamic_entry(DT_SYMENT, 24);
    push_dynamic_entry(DT_NULL, 0);
    push_dynamic_entry(DT_NULL, 0);
    push_dynamic_entry(DT_NULL, 0);
    push_dynamic_entry(DT_NULL, 0);
    push_dynamic_entry(DT_NULL, 0);
    push_dynamic_entry(DT_NULL, 0);
}

static void push_dynstr() {
    push_byte(0);
    push_string("foo");
}

static void push_dynsym() {
    // TODO: Some of these can be turned into enums using https://docs.oracle.com/cd/E19683-01/816-1386/chapter6-79797/index.html
    push_symbol(0, 0, 0, 0, 0, 0); // "<null>"
    push_symbol(1, 0x60010, 0x2000, 0, 0, 0); // "foo"
}

static u32 get_nbucket(size_t symbol_count) {
    // From https://sourceware.org/git/?p=binutils-gdb.git;a=blob;f=bfd/elflink.c;h=6db6a9c0b4702c66d73edba87294e2a59ffafcf5;hb=refs/heads/master#l6560
    //
    // Array used to determine the number of hash table buckets to use
    // based on the number of symbols there are. If there are fewer than
    // 3 symbols we use 1 bucket, fewer than 17 symbols we use 3 buckets,
    // fewer than 37 we use 17 buckets, and so forth. We never use more
    // than MAX_BUCKETS (32771) buckets.
    static const u32 nbucket_options[] = {
        1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209, 16411, MAX_BUCKETS, 0
    };

    u32 nbucket = 0;

    for (size_t i = 0; nbucket_options[i] != 0; i++) {
        nbucket = nbucket_options[i];

        if (symbol_count < nbucket_options[i + 1]) {
            break;
        }
    }

    return nbucket;
}

// From https://sourceware.org/git/?p=binutils-gdb.git;a=blob;f=bfd/elf.c#l193
static u32 elf_hash(const char *namearg) {
    u32 h = 0;

    for (const unsigned char *name = (const unsigned char *) namearg; *name; name++) {
        h = (h << 4) + *name;
        h ^= (h >> 24) & 0xf0;
    }

    return h & 0x0fffffff;
}

static void push_chain(u32 chain) {
    if (chains_size + 1 > MAX_SYMBOLS) {
        fprintf(stderr, "error: MAX_SYMBOLS of %d was exceeded\n", MAX_SYMBOLS);
        exit(EXIT_FAILURE);
    }

    chains[chains_size++] = chain;
}

// See https://flapenguin.me/elf-dt-hash
// See https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.dynamic.html#hash
//
// Example with 16 symbols "abcdefghijklmnop":
// 
// nbuckets: 3 (what get_nbucket() returns when there are 16 symbols)
// nchain: 17 (16 symbols + the SHT_UNDEF at index 0)
// 
// Bucket[i] always has the value of the last entry that has `hash % nbucket` equal to `i`
// 
//  i  bucket[i]  name of first symbol in chain
// --  ---------  -----------------------------
//  0  11         c
//  1  16         m
//  2  15         e
// 
// Two asterisks ** and parens () indicate the start of a chain, so it's easier to see.
// 
//        SYMBOL TABLE   |
//                       |
// 	   name =            | hash =          bucket_index =
//  i  symtab[i].st_name | elf_hash(name)  hash % nbucket
// --  ----------------- | --------------  --------------  
//  0  <STN_UNDEF>       |
//  1  b                 |  98             2                 /---> 0
//  2  p                 | 112             1                 | /-> 0
//  3  j                 | 106             1                 | \-- 2 <---\
//  4  n                 | 110             2                 \---- 1 <---|-\
//  5  f                 | 102             0                       0 <-\ | |
//  6  g                 | 103             1                 /---> 3 --|-/ |
//  7  o                 | 111             0                 | /-> 5 --/   |
//  8  l                 | 108             0                 | \-- 7 <-\   |
//  9  k                 | 107             2               /-|---> 4 --|---/
// 10  i                 | 105             0               | | /-> 8 --/
// 11  c                 |  99             0 **            | | \-(10)
// 12  d                 | 100             1               | \---- 6 <-\
// 13  h                 | 104             2               \------ 9 <-|-\
// 14  a                 |  97             1                  /-> 12 --/ |
// 15  e                 | 101             2 **               |  (13)----/
// 16  m                 | 109             1 **               \--(14)
static void push_hash(char *symbols[]) {
    size_t symbol_count = 16; // TODO: Turn this into symbols_size, tracking the length of the global symbols array

    u32 nbucket = get_nbucket(symbol_count);
    push_number(nbucket, 4);

    u32 nchain = 1 + symbol_count; // `1 + `, because index 0 is always STN_UNDEF (the value 0)
    push_number(nchain, 4);

    static u32 buckets[MAX_BUCKETS];

    memset(buckets, 0, nbucket * sizeof(u32));

    for (size_t i = 0; i < symbol_count; i++) {
        u32 hash = elf_hash(symbols[i]);
        u32 bucket_index = hash % nbucket;

        push_chain(buckets[bucket_index]);

        buckets[bucket_index] = i + 1;
    }

    for (size_t i = 0; i < nbucket; i++) {
        push_number(buckets[i], 4);
    }

    push_number(0, 4); // The first entry in the chain is always STN_UNDEF

    for (size_t i = 0; i < chains_size; i++) {
        push_number(chains[i], 4);
    }

    // push_zeros(4); // Alignment
}

static void push_section(u32 name_offset, u32 type, u64 flags, u64 address, u64 offset, u64 size, u32 link, u32 info, u64 alignment, u64 entry_size) {
    push_number(name_offset, 4);
    push_number(type, 4);
    push_number(flags, 8);
    push_number(address, 8);
    push_number(offset, 8);
    push_number(size, 8);
    push_number(link, 4);
    push_number(info, 4);
    push_number(alignment, 8);
    push_number(entry_size, 8);
}

static void push_section_headers() {
    // Null section
    // 0x20e0 to 0x2120
    push_zeros(0x40);

    // .hash: Hash section
    // 0x2120 to 0x2160
    push_section(0x1b, SHT_HASH, SHF_ALLOC, 0x120, 0x120, 0x14, 2, 0, 8, 4);

    // .dynsym: Dynamic linker symbol table section
    // 0x2160 to 0x21a0
    push_section(0x21, SHT_DYNSYM, SHF_ALLOC, 0x138, 0x138, 0x30, 3, 1, 8, 0x18);

    // .dynstr: String table section
    // 0x21a0 to 0x21e0
    push_section(0x29, SHT_STRTAB, SHF_ALLOC, 0x168, 0x168, 5, 0, 0, 1, 0);

    // .eh_frame: Program data section
    // 0x21e0 to 0x2220
    push_section(0x31, SHT_PROGBITS, SHF_ALLOC, 0x1000, 0x1000, 0, 0, 0, 8, 0);

    // .dynamic: Dynamic linking information section
    // 0x2220 to 0x2260
    push_section(0x3b, SHT_DYNAMIC, SHF_WRITE | SHF_ALLOC, 0x1f50, 0x1f50, 0xb0, 3, 0, 8, 0x10);

    // .data: Data section
    // 0x2260 to 0x22a0
    push_section(0x44, SHT_PROGBITS, SHF_WRITE | SHF_ALLOC, 0x2000, 0x2000, 4, 0, 0, 4, 0);

    // .symtab: Symbol table section
    // 0x22a0 to 0x22e0
    // "link" of 8 is the section header index of the associated string table; see https://blog.k3170makan.com/2018/09/introduction-to-elf-file-format-part.html
    // "info" of 4 is one greater than the symbol table index of the last local symbol (binding STB_LOCAL)
    push_section(1, SHT_SYMTAB, 0, 0, 0x2008, 0x78, 8, 4, 8, 0x18);

    // .strtab: String table section
    // 0x22e0 to 0x2320
    push_section(0x09, SHT_PROGBITS | SHT_SYMTAB, 0, 0, 0x2080, 0x14, 0, 0, 1, 0);

    // .shstrtab: Section header string table section
    // 0x2320 to end
    push_section(0x11, SHT_PROGBITS | SHT_SYMTAB, 0, 0, 0x2094, 0x4a, 0, 0, 1, 0);
}

static void push_program_header(u32 type, u32 flags, u64 offset, u64 virtual_address, u64 physical_address, u64 file_size, u64 mem_size, u64 alignment) {
    push_number(type, 4);
    push_number(flags, 4);
    push_number(offset, 8);
    push_number(virtual_address, 8);
    push_number(physical_address, 8);
    push_number(file_size, 8);
    push_number(mem_size, 8);
    push_number(alignment, 8);
}

static void push_elf_header() {
    // Magic number
    // 0x0 to 0x4
    push_byte(0x7f);
    push_byte('E');
    push_byte('L');
    push_byte('F');

    // 64-bit
    // 0x4 to 0x5
    push_byte(2);

    // Little-endian
    // 0x5 to 0x6
    push_byte(1);

    // Version
    // 0x6 to 0x7
    push_byte(1);

    // SysV OS ABI
    // 0x7 to 0x8
    push_byte(0);

    // Padding
    // 0x8 to 0x10
    push_zeros(8);

    // Shared object
    // 0x10 to 0x12
    push_byte(ET_DYN);
    push_byte(0);

    // x86-64 instruction set architecture
    // 0x12 to 0x14
    push_byte(0x3E);
    push_byte(0);

    // Original version of ELF
    // 0x14 to 0x18
    push_byte(1);
    push_zeros(3);

    // No execution entry point address
    // 0x18 to 0x20
    push_zeros(8);

    // Program header table offset
    // 0x20 to 0x28
    push_byte(0x40);
    push_zeros(7);

    // Section header table offset
    // 0x28 to 0x30
    push_byte(0xe0);
    push_byte(0x20);
    push_zeros(6);

    // Processor-specific flags
    // 0x30 to 0x34
    push_zeros(4);

    // ELF header size
    // 0x34 to 0x36
    push_byte(0x40);
    push_byte(0);

    // Single program header size
    // 0x36 to 0x38
    push_byte(0x38);
    push_byte(0);

    // Number of program header entries
    // 0x38 to 0x3a
    push_byte(4);
    push_byte(0);

    // Single section header entry size
    // 0x3a to 0x3c
    push_byte(0x40);
    push_byte(0);

    // Number of section header entries
    // 0x3c to 0x3e
    push_byte(10);
    push_byte(0);

    // Index of entry with section names
    // 0x3e to 0x40
    push_byte(9);
    push_byte(0);
}

static void reset() {
    bytes_size = 0;
    chains_size = 0;
}

static void generate_simple_so() {
    reset();

    FILE *f = fopen("foo.so", "w");
    if (!f) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // 0x0 to 0x40
    push_elf_header();

    // 0x40 to 0x120
    push_program_header(PT_LOAD, PF_R, 0, 0, 0, 0x1000, 0x1000, 0x1000);
    push_program_header(PT_LOAD, PF_R | PF_W, 0x1f50, 0x1f50, 0x1f50, 0xb4, 0xb4, 0x1000);
    push_program_header(PT_DYNAMIC, PF_R | PF_W, 0x1f50, 0x1f50, 0x1f50, 0xb0, 0xb0, 8);
    push_program_header(0x6474e552, PF_R, 0x1f50, 0x1f50, 0x1f50, 0xb0, 0xb0, 1);

    char *symbols[] = {
        "b",
        "p",
        "j",
        "n",
        "f",
        "g",
        "o",
        "l",
        "k",
        "i",
        "c",
        "d",
        "h",
        "a",
        "e",
        "m",
    };

    // 0x120 to 0x134
    push_hash(symbols);

    // 0x138 to 0x168
    push_dynsym();

    // 0x168 to 0x16d
    push_dynstr();

    // TODO: REMOVE!
    // 0x16d to 0x1f50
    push_zeros(3); // Alignment
    push_zeros(0x1de0);

    // 0x1f50 to 0x2000
    push_dynamic();

    // 0x2000 to 0x2008
    push_data();

    // 0x2008 to 0x2080
    push_symtab();

    // 0x2080 to 0x2094
    push_strtab();

    // 0x2094 to 0x20e0
    push_shstrtab();

    // 0x20e0 to end
    push_section_headers();

    fwrite(bytes, sizeof(u8), bytes_size, f);

    fclose(f);
}

int main() {
    generate_simple_so();
}