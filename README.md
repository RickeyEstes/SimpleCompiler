# SimpleCompiler

This is the final school work of
Compiler Technology, School of Computer Science and Engineering, BUAA.

This simple compiler compiles source code of a modified and simplified C language into MIPS assembly code.

Badly-organized. No inline optimization support.

# Usage

## Build

```
make BUILD_TYPE=RELEASE
```

## Compile

There is a sample program in `sample/test.txt`. An equality in C language is in `sample/test.c`.

```
./main sample/test.txt test.quad test.asm test.opt.quad test.opt.asm
```
