# Y86 assembler and simulator

## About Y86

Y86 is a simplified x86-like instruction set.

### 8 user-accessible registers:

`EAX`, `ECX`, `EDX`, `EBX`, `ESP`, `EBP`, `ESI`, `EDI`

### 4 states:

`AOK`, `HLT`, `ADR`, `INS`

### Instructions:

`halt`, `nop`, `rrmovl(cmovXX)`, `irmovl`, `rmmovl`, `mrmovl`,
`OPl`, `jXX`, `call`, `ret`, `pushl`, `popl`

`OP`: `add`, `sub`, `and`, `xor`

`XX`: `all`, `le`, `l`, `e`, `ne`, `ge`, `g`


### ASM file example:

see [example](./example)

For more details: Chapter 4 of [CSAPP](http://csapp.cs.cmu.edu/)

## Y86 Assembler

Build:

`make Y86asm`

Run:

`Y86asm <input> [<output>]`

use `y.out` by default if `<output>` is not specified.

## Y86 Simulator

Build:

`make Y86sim`

Run:

`Y86sim <input>`

## License

MIT License
