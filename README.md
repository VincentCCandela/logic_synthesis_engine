# runtime_steps.txt is an abbreviated version of this readme.

# Logic Synthesis Engine Description

This is a command-line Boolean function minimizer written in C++17. Given an expression like ab + c'd + ab', this code prints the function's: Canonical SOP, Canonical POS, Inverse canonical SOP, Inverse canonical POS, Literal-minimized SOP, Literal-minimized POS, Prime implicants, Essential prime implicants, On-set minterms, Off-set maxterms, and generates gate-level Verilog, and a Quartus project that put the function on the Terasic Cyclone V GX Starter Kit FPGA. The variables become slide switches from left to right and the result lights the last switch-LED.

## Architecture overview

`main.cpp` contains all of the code which uses cpp17 standard library. It coordinates commandline parsing, file loading, selects the input format, runs the analysis, prints results to stdout, and writes output to files.

Dataflow consists like so:

expression (one line on stdin)
1) clean input and check parentheses: main()
2) evaluate all inputs using recursive flow: evaluate_sum to evaluate_product to evaluate_factor
3) create truth tables of F and F': main()
4) calculate the prime implicants: primes(), implicant()
5) calculate the essential and non-essential primes: split(), is_essential()
6) calculate the minimum-literal cover: minimize(), coversAll(), literals()
7) print the results: sop(), pos(), products(), numbers()
8) create verilog code sop.v, sop.qsf, sop.qpf: verilog(), quartus_settings(), write_file()
9) Truth table is printed to .csv using truth_table_csv(), and longest path delay is calculated via delay()


Representation: A product term is a `Cube` with two bit masks over the variables: `mask` marks which variables appear and `value` indicates their value (1 = plain, 0 = inverted) eg for variables `a b c`, `ab'` has mask `110` and value `100`. Variables are sorted alphabetically, with the first as the most significant bit and the last as bit 0. Therefore, a minterm is just an integer, and a cube covers minterm `m` when `(m & mask) == value`. This trick works because the mask will have less ones than m, and we check that the value of the cube is the exact same as the value of the minterm.

Parser: The commandline parser is a recursive-descent evaluator with one function per grammar level. Instead of building a syntax tree, it evaluates the expression directly for each of the 2^n inputs to fill in the truth table. The BLIF parser builds nodes by locating the ".name" directory and intepreting its behavior using the truth table below it. Output values are evaluated recursively to propagate determined inputs forward. Specifically, we store a (signal, value) map for all inputs, and compares inputs of nodes to values in the map. If the input is found (circuit inputs, or previously evaluated ones), we return; otherwise, we evaluate the node whose output is the unknown input. The overall truth table is constructed by evaluating the output of each input combination.

NOTE: adjacent factors are ANDed

Minimization:
1) Prime implicants: each of the possible product terms is checked. A term is an implicant if it covers no off-set minterm, and prime if dropping any one of its literals would make it cover one.
2) Essential primes: a prime is essential if it is the only prime covering some minterm.
3) Cover: the essential primes are combined with every subset of the non-essential primes. The cover of all on-set minterms with the fewest literals is chosen.
4) POS: steps 1–3 run on F', and De Morgan's law is used to calculate the complements of each product term.

Hardware generation. verilog() creates structural code using gate primitives. `sop_gates` has one input per variable, a `not` gate for each variable that appears complemented, an `and` gate for each product term with two or more literals, and an `or` gate that drives `out`.`buf` is used when there is only one term. `sop` is the top level for the board, with input `SW[n-1:0]` and output `LEDR[0]`.

quartus_settings() writes the FPGA device 5CGXFC5C6F27C7, the top-level entity, the Verilog file, and the pin assignments from the Terasic C5G user manual.

## Pins

```
Signal; FPGA pins; I/O standard
SW[0] to SW[9]; AC9, AE10, AD13, AC8, W11, AB10, V10, AC10, Y11, AE19; 1.2 V
LEDR[0]; F7; 2.5 V
```

## Repository folder structure

```
logic_synthesis_engine
  main.cpp: all of the synthesizer code, Verilog and Quartus writers
  parser.cpp and parser.h: parser code for graphical reprentation of circuit given in .blif files
  runtime_steps.txt: simplified demonstration of runtime commands
  README.md: this file
  .gitignore: no compile time and generated files allowed in git
  Examples: contains example inputs
```

## Build, run and usage

A cpp17 compiler is needed. Quartus prime lite edition is needed with `quartus/bin` in the executable path, and a connected terasic cyclone v gx starter kit via usb blaster is also needed. 

This setup used g++17 with Quartus Prime Lite 24.1std on Ubuntu 24.04.

### Step 1: Build

There are two builds. The simple build reads an expression piped in on stdin and only needs `main.cpp`:

```bash
git clone https://github.com/VincentCCandela/logic_synthesis_engine.git
cd logic_synthesis_engine
make simple
```

The extended build also reads BLIF circuit files. It defines `BLIF` and adds `parser.cpp`:

```bash
make blif
```

Both write `./lse`.

### Step 2: Run the minimizer

#### Commandline Boolean Expression Mode
Pipe in an expression written with the below command

```bash
echo "ab + c'd + ab'" | ./lse
```

Important: Only parentheses, single char variables, +, and ' can be used.

Spaces are ignored. Variables are ordered alphabetically, with capital letters before lowercase ones. Unbalanced parentheses are accounted for via error message.

Outputs are printed to terminal except for the truth table which is saved to .csv 

Example
```
Canonical SOP: F=a'b'c'd + a'bc'd + ab'c'd' + ab'c'd + ab'cd' + ab'cd + abc'd' + abc'd + abcd' + abcd
Canonical POS: F=(a + b + c + d)(a + b + c' + d)(a + b + c' + d')(a + b' + c + d)(a + b' + c' + d)(a + b' + c' + d')
Inverse canonical SOP: F'=a'b'c'd' + a'b'cd' + a'b'cd + a'bc'd' + a'bcd' + a'bcd
Inverse canonical POS: F'=(a + b + c + d')(a + b' + c + d')(a' + b + c + d)(a' + b + c + d')(a' + b + c' + d)(a' + b + c' + d')(a' + b' + c + d)(a' +b' + c + d')(a' + b' + c' + d)(a' + b' + c' + d')
Literal-minimized SOP: F=c'd + a
Literal-minimized POS: F=(a + d)(a + c')
Prime implicants: c'd, a
Essential prime implicants: c'd, a
On-set minterms: m(1, 5, 8, 9, 10, 11, 12, 13, 14, 15)
Off-set maxterms: M(0, 2, 3, 4, 6, 7)
Estimated delay (gates on the longest path): 3
Truth table: wrote truth_table.csv
Structural Verilog: wrote sop.v, open sop.qpf in Quartus to put it on the Cyclone V GX board
```
#### Graphical Circuit Mode
For graphical circuit mode, use the extended build (`make blif`), and pass in a BLIF file in the arguments when running the executable. The simple build prints an error if it is given a file.

```sh
./lse Examples/adder.blif
```

The program follows the BLIF format convention, including the combinational `.names` format with `.model`, `.inputs`, `.outputs`, truth-table rows, and `.end`. For example:

```blif
.model simple
.inputs a b
.outputs f
.names a b f
11 1
.end
```

The program chooses BLIF parsing when the input filename is passed as an argument and ends in `.blif`. Sequential BLIF elements and other advanced BLIF directives are not supported.

NOTE: For the blif  runtime, the only function that reaches the board is the minimized SOP of the last name in .outputs.

### Step 3: Check that `sop.v` holds the new function

```bash
head -1 sop.v
```

This should print the minimized sop.

#### Structural Verilog output

The program writes one Verilog file per circuit output in the current working directory. Each file contains synthesizable combinational modules named after the output. Its input ports are the circuit's input nets, and its output port is the corresponding output net. The module is structural gate-level Verilog generated from the minimized SOP:

- `not` gates invert complemented input literals.
- `and` gates form each product term.
- An `or` gate combines the product terms into the output.
- Internal wires connect the gates, and the default name for these connections are `p<number>` for products and `<signal>_n` for complementes.

For example, an SOP such as `f = a' b + a c` is represented using an inverter for `a`, two AND gates for the products, and an OR gate driving `f`.


### Step 4: Compile with Quartus

```bash
quartus_sh --flow compile sop
```

### Step 5: Program the board

```bash
quartus_pgm -c 1 -m jtag -o "p;sop.sof@1"
```

 In the command, `-c 1` selects the first programming cable and `@1` the first device on its JTAG chain. Use `jtagconfig` might be helpful if the board is not found.

### Step 6: Try it on the switches

1) Inputs: the variables go on the slide switches in alphabetical order with the last one being `SW0`. So `a b c d`, means a=SW3, b=SW2, c=SW1 and d=SW0. Up is 1 and down is 0.
2) Output:`LEDR0` shows F, and on means 1.

Repeat steps 2-5 to generate new verilog code and flash the board.

### Notes

1) 10 variables, 1 per switch. 
2) Exhaustive cover search.
3) Only parentheses, single char variables, +, and ' can be used.
4) For a function that is always 0, the minimized POS prints as `F=()`.
5) AI was used for concept understanding, drafting the code, and error-checking the code, and setup of code runtimes
