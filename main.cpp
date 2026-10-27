/*
Implementation of the Quine–McCluskey algorithm
AI was used for concept understanding, drafting the code, and error-checking the code, and setup of code runtimes
*/
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "parser.h"

struct Cube
{
    int mask;  // which variables are being used
    int value; // 1 = plain, 0 = complemented
};

std::string variables; // variables in alphabetical order, the first one is the most significant bit
int max_minterm;       // mask with a bit set for every variable
int num_vars;

// fpga pins of the slide switches sw[0] to sw[9] on the cyclone v gx starter kit
std::vector<std::string> switch_pins{"AC9", "AE10", "AD13", "AC8", "W11", "AB10", "V10", "AC10", "Y11", "AE19"};

// which bit holds the variable, the last variable is bit 0
int position(char variable)
{
    return variables.size() - 1 - variables.find(variable);
}

// calculate one-hot encodings
int bit(char variable)
{
    return 1 << position(variable);
}

// shorter version
//  check that the relevant values of minterm (cut out the literals not in the cube) are the same as the cube's values
bool covers(Cube cube, int minterm)
{
    return (minterm & cube.mask) == cube.value;
}

// // does the minterm belong to the cube?
// bool covers(Cube cube, int minterm){
//     for(int i = 0; i < num_vars; i++){
//         //skip variables outside the cube
//         if((cube.mask >> i) & 1 == 0){
//             continue;
//         }
//         //the minterm's bit must match the the cube bit
//         int have = (minterm >> i) & 1;
//         int want = (cube.value >> i) & 1;
//         if(have != want){
//             return false;
//         }
//     }
//     return true;
// }

// turns a Cube (a product term) back into readable text like ab'c
//  the reverse of parsing
std::string product(Cube term)
{
    std::string text;
    for (char variable : variables)
    {
        if (term.mask & bit(variable))
        {
            text += variable;
            if (!(term.value & bit(variable)))
            {
                text += "'";
            }
        }
    }
    return text.empty() ? "1" : text;
}

// de morgan complement of a product term ex: ab' -> (a' + b)
std::string sum(Cube term)
{
    std::string text;
    for (char variable : variables)
    {
        if (term.mask & bit(variable))
        {
            if (!text.empty())
            {
                text += " + ";
            }
            text += variable;
            if (term.value & bit(variable))
            {
                text += "'";
            }
        }
    }
    return "(" + text + ")";
}

// the product terms joined by `separator`, or `empty_text` if there are none
std::string products(const std::vector<Cube> &cubes, std::string separator, std::string empty_text)
{
    std::string text;
    for (Cube cube : cubes)
    {
        if (!text.empty())
        {
            text += separator;
        }
        text += product(cube);
    }
    return text.empty() ? empty_text : text;
}

// turns each cube into a product term like ab' and joins the terms with +
std::string sop(const std::vector<Cube> &cubes)
{
    return products(cubes, " + ", "0");
}

// writes the complements of the product term using De Morgan’s law
std::string pos(const std::vector<Cube> &cubes)
{
    std::string text;
    for (Cube cube : cubes)
    {
        text += sum(cube);
    }
    return text.empty() ? "1" : text;
}

// turns a list of integers into a comma-separated string
std::string numbers(const std::vector<int> &values)
{
    std::string text;
    for (int value : values)
    {
        if (!text.empty())
        {
            text += ", ";
        }
        text += std::to_string(value);
    }
    return text;
}

// the names joined by `separator`
std::string join(const std::vector<std::string> &names, std::string separator)
{
    std::string text;
    for (const std::string &name : names)
    {
        if (!text.empty())
        {
            text += separator;
        }
        text += name;
    }
    return text;
}

// each minterm number as a cube that contains every variable
std::vector<Cube> minterms(const std::vector<int> &minterm_numbers)
{
    std::vector<Cube> result;
    for (int minterm : minterm_numbers)
    {
        result.push_back({max_minterm, minterm});
    }
    return result;
}

// true if cube covers only on-set minterms of truth_table
bool implicant(Cube cube, const std::vector<bool> &truth_table)
{
    for (int minterm = 0; minterm <= max_minterm; ++minterm)
    {
        // cube covers the minterm and the minterm is in the off-set so it is not an implicant
        if (covers(cube, minterm) && !truth_table[minterm])
        {
            return false;
        }
    }
    return true;
}

// the cubes that cover only on-set minterms of truth_table and cannot drop any literal while staying that way
std::vector<Cube> primes(const std::vector<bool> &truth_table)
{
    std::vector<Cube> result;
    // go through all the masks, and all the values
    // make sure that they are in the on-set AND that they don't contain excess literals
    // then, the implicant is a prime!
    for (int mask = 0; mask <= max_minterm; ++mask)
    {
        for (int value = 0; value <= max_minterm; ++value)
        {
            // skips 1 values that have a corresponding 0 in the mask (skip offset minterms)
            if ((value & ~mask))
            {
                continue;
            }
            // implicant is not in on-set
            else if (!implicant({mask, value}, truth_table))
            {
                continue;
            }
            // cube is prime if dropping any of its literals makes it cover an off-set minterm
            bool prime = true;
            for (char variable : variables)
            {
                if (!(mask & bit(variable)))
                { // var not in term
                    continue;
                }
                Cube variable_removed = {mask & ~bit(variable), value & ~bit(variable)};
                // original could have been bigger (did not need the literal)
                if (implicant(variable_removed, truth_table))
                {
                    prime = false;
                    break;
                }
            }
            if (prime)
            {
                result.push_back({mask, value});
            }
        }
    }
    return result;
}

// true if prime is the only cube in prime_implicants covering some minterm
bool is_essential(Cube prime, const std::vector<Cube> &prime_implicants)
{
    // go through the minterms and check that
    // multiple prime implicnats do not cover the same minterm
    for (int minterm = 0; minterm <= max_minterm; ++minterm)
    {
        if (!covers(prime, minterm))
        {
            continue;
        }
        int times_covered = 0;
        for (Cube cube : prime_implicants)
        {
            if (covers(cube, minterm))
            {
                ++times_covered;
            }
        }
        if (times_covered == 1)
        {
            return true;
        }
    }
    return false;
}

// true if every on-set minterm of truth_table is covered by some cube in cover
bool coversAll(const std::vector<Cube> &cover, const std::vector<bool> &truth_table)
{
    for (int minterm = 0; minterm <= max_minterm; ++minterm)
    {
        if (!truth_table[minterm])
        { // is irrelevant(false bc not in onset)
            continue;
        }
        bool covered = false;
        for (const Cube &cube : cover)
        {
            if (covers(cube, minterm))
            {
                covered = true;
                break;
            }
        }
        if (!covered)
        {
            return false;
        }
    }
    return true;
}

// calculate the number of literals in the cubes
// ie ab' + c has 3
int literal(const std::vector<Cube> &cubes)
{
    int count = 0;
    for (Cube cube : cubes)
    {
        for (char variable : variables)
        {
            if (cube.mask & bit(variable))
            {
                ++count;
            }
        }
    }
    return count;
}

// what is an essential prime implicant and what is not
void split(const std::vector<Cube> &prime_implicants, std::vector<Cube> &essential_primes, std::vector<Cube> &nonessential_primes)
{
    for (Cube prime : prime_implicants)
    {
        if (is_essential(prime, prime_implicants))
        {
            essential_primes.push_back(prime);
        }
        else
        {
            nonessential_primes.push_back(prime);
        }
    }
}

std::vector<Cube> minimize(const std::vector<bool> &truth_table, const std::vector<Cube> &essential_primes, const std::vector<Cube> &nonessential_primes)
{
    std::vector<Cube> best = essential_primes;
    for (Cube prime : nonessential_primes)
    {
        best.push_back(prime);
    }

    // go through every possible combination of nonessential
    // prime-implicants to add to the essentials
    // cover needs cover the minterms in the truth table AND
    // have less literals than the previous best cover
    for (int subset = 0; subset < 1 << nonessential_primes.size(); ++subset)
    {
        std::vector<Cube> cover = essential_primes;
        for (size_t i = 0; i < nonessential_primes.size(); ++i)
        {
            if ((subset >> i) & 1)
            { // in cover
                cover.push_back(nonessential_primes[i]);
            }
        }
        if (coversAll(cover, truth_table) && literal(cover) < literal(best))
        {
            best = cover;
        }
    }
    return best;
}

// the verilog wire that carries the variable's literal in term (a' is carried by a_n)
std::string wire(Cube term, char variable)
{
    std::string name(1, variable);
    if (!(term.value & bit(variable)))
    { // indicate negation
        name += "_n";
    }
    return name;
}

// adds a gate like `and g1(p0, a, b_n);` to gates, the gate is named after its place in the list
void add_gate(std::vector<std::string> &gates, std::string type, std::string output, std::string inputs)
{
    gates.push_back(type + " g" + std::to_string(gates.size()) + "(" + output + ", " + inputs + ");");
}

// create not, and, or, and buf gates
// module sop_gates holds gates, module sop is top level module for the cyclone v gx starter kit
// each variable is a slide switch and last variable is on sw[0] (switches spell out the minterm number)
// the output is on ledr[0]
std::string verilog(const std::vector<Cube> &cubes, int output, std::vector<std::string> *connections)
{
    std::vector<std::string> ports;
    std::vector<std::string> wires; // the internal signals that have to be declared with wire
    std::vector<std::string> gates; // the full text of every gate line in the order they were added
    for (char variable : variables)
    {
        std::string name(1, variable);
        ports.push_back("input " + name);
        connections->push_back("." + name + "(SW[" + std::to_string(position(variable)) + "])");

        for (Cube cube : cubes)
        {
            if ((cube.mask & bit(variable)) && !(cube.value & bit(variable)))
            {
                wires.push_back(name + "_n");
                add_gate(gates, "not", name + "_n", name);
                break;
            }
        }
    }
    ports.push_back("output out");
    connections->push_back(".out(LEDR[" + std::to_string(output) + "])");

    std::vector<std::string> terms; // one entry per product term, naming the signal that carries it
    for (Cube cube : cubes)
    {
        std::vector<std::string> factors; // literals of one product term, as wire names
        for (char variable : variables)
        {
            if (cube.mask & bit(variable))
            {
                factors.push_back(wire(cube, variable));
            }
        }
        if (factors.empty())
        {
            terms.push_back("1'b1");
        }
        else if (factors.size() == 1)
        {
            terms.push_back(factors[0]);
        }
        else
        {
            std::string product = "p" + std::to_string(terms.size());
            wires.push_back(product);
            add_gate(gates, "and", product, join(factors, ", "));
            terms.push_back(product);
        }
    }

    if (terms.empty())
    {
        terms.push_back("1'b0");
    }
    if (terms.size() == 1)
    {
        add_gate(gates, "buf", "out", terms[0]);
    }
    else
    {
        add_gate(gates, "or", "out", join(terms, ", "));
    }

    std::string text = "//structural verilog for f = " + sop(cubes) + "\n";
    text += "module sop_gates_" + std::to_string(output) + "(\n    " + join(ports, ",\n    ") + "\n);\n";
    if (!wires.empty())
    {
        text += "    wire " + join(wires, ", ") + ";\n";
    }
    text += "    " + join(gates, "\n    ") + "\n";
    text += "endmodule\n\n";
    return text;
}

std::string verilog_top(std::vector<std::vector<std::string>> all_connections)
{
    std::string text = "//top level entity for the cyclone v gx starter kit\n";
    for (size_t i = 0; i < all_connections.size(); i++)
    {
        text += "`include \"sop_" + std::to_string(i) + ".v\"\n";
    }
    std::vector<std::string> pins;
    if (num_vars > 0)
    {
        pins.push_back("input [" + std::to_string(num_vars - 1) + ":0] SW");
    }
    pins.push_back("output [" + std::to_string(all_connections.size() - 1) + ":0] LEDR");
    text += "module sop(\n    " + join(pins, ",\n    ") + "\n);\n";
    for (size_t i = 0; i < all_connections.size(); i++)
    {
        text += "    sop_gates_" + std::to_string(i) + " u" + std::to_string(i) + "(\n        " + join(all_connections[i], ",\n        ") + "\n    );\n";
    }

    text += "endmodule\n";
    return text;
}

// quartus settings that put module sop on the cyclone v gx starter kit
// the pins and i/o standards come from the terasic c5g user manual
std::string quartus_settings()
{
    std::string text;
    text += "set_global_assignment -name FAMILY \"Cyclone V\"\n";
    text += "set_global_assignment -name DEVICE 5CGXFC5C6F27C7\n";
    text += "set_global_assignment -name TOP_LEVEL_ENTITY sop\n";
    text += "set_global_assignment -name VERILOG_FILE sop.v\n";
    for (int i = 0; i < num_vars; ++i)
    {
        std::string sw = "SW[" + std::to_string(i) + "]";
        text += "set_location_assignment PIN_" + switch_pins[i] + " -to " + sw + "\n";
        text += "set_instance_assignment -name IO_STANDARD \"1.2 V\" -to " + sw + "\n";
    }
    text += "set_location_assignment PIN_F7 -to LEDR[0]\n";
    text += "set_instance_assignment -name IO_STANDARD \"2.5 V\" -to LEDR[0]\n";
    return text;
}

// writes text into the file called name, false if it could not be written
bool write_file(std::string name, std::string text)
{
    std::ofstream file(name);
    file << text;
    return file.good();
}

// the expression is read by three functions that call each other
// a sum is products joined by +, like ab'+c
// a product is factors written next to each other, like a(b+c)'
// a factor is a variable or a sum in parentheses, followed by any number of ' marks
// each function returns the value of what it read for one minterm and moves index past it

// declared here because evaluate_factor calls it for a sum in parentheses
bool evaluate_sum(const std::string &expression, size_t &index, int minterm);

// reads one factor: a, b', or (a+b)'
bool evaluate_factor(const std::string &expression, size_t &index, int minterm)
{
    bool value;
    if (expression[index] == '(')
    {
        ++index; // skip the (
        value = evaluate_sum(expression, index, minterm);
        ++index; // skip the )
    }
    else
    {
        value = minterm & bit(expression[index]); // true if variable is 1 in minterm
        ++index;
    }
    // complement
    while (index < expression.size() && expression[index] == '\'')
    { // value is 0 if inverted
        value = !value;
        ++index;
    }
    return value;
}

// reads factors until a plus, closing parenthesis or the end and ands them together
// a product with no factors, from a stray + like in a++b, is 0 so it adds nothing to the sum
bool evaluate_product(const std::string &expression, size_t &index, int minterm)
{
    bool value = true;
    bool empty = true;
    // increment index for starting complement (ignore it) otherwise let evaluate_factor() increment index
    while (index < expression.size() && expression[index] != '+' && expression[index] != ')')
    {
        if (expression[index] == '\'')
        {
            ++index;
        }
        else
        {
            bool factor = evaluate_factor(expression, index, minterm);
            value = value && factor;
            empty = false;
        }
    }
    return value && !empty;
}

// reads products separated by +, and ors them together
bool evaluate_sum(const std::string &expression, size_t &index, int minterm)
{
    bool value = evaluate_product(expression, index, minterm);
    while (index < expression.size() && expression[index] == '+')
    {
        ++index;
        bool term = evaluate_product(expression, index, minterm);
        value = value || term;
    }
    return value;
}

// the truth table as csv text, a column for each variable and one for f, a row for each minterm
std::string truth_table_csv(const std::vector<bool> &truth_table)
{
    std::string text;
    for (char variable : variables)
    {
        text += variable;
        text += ",";
    }
    text += "F\n";
    for (int minterm = 0; minterm <= max_minterm; ++minterm)
    {
        for (char variable : variables)
        {
            text += (minterm & bit(variable)) ? "1," : "0,";
        }
        text += truth_table[minterm] ? "1\n" : "0\n";
    }
    return text;
}

// estimated delay of the minimized sop circuit, counted as the gates on its slowest path from a switch to out
// every gate counts as one delay, the buf is skipped since it only copies its input
// this works for the minimized sop, not for the full non-minimized function
int delay(const std::vector<Cube> &cubes)
{
    int longest = 0;
    for (Cube cube : cubes)
    {
        int gates = 0;
        if (cube.value != cube.mask)
        { // not gate, a complemented literal is 1 in mask but 0 in value
            ++gates;
        }
        if (literal({cube}) > 1)
        { // and gate
            ++gates;
        }
        if (gates > longest)
        {
            longest = gates;
        }
    }
    if (cubes.size() > 1)
    { // or gate
        ++longest;
    }
    return longest;
}

int main(int argc, char *argv[])
{
    int mode = argc == 2; // mode 0 == commandline mode, 1 == BLIF file mode

    std::vector<std::vector<bool>> truth_tables;
    std::vector<std::string> outputs; // outputs
    // BLIF: input parsing
    if (mode == 1)
    {
        std::string filename = argv[1];
        std::ifstream inFile(filename);
        std::string line;
        std::vector<std::string> lines;
        while (getline(inFile, line))
            lines.push_back(line);
        inFile.close();

        if (filename.length() >= 5 && filename.compare(filename.length() - 5, 5, ".blif") != 0)
        {
            cout << "invalid BLIF file" << endl;
            return -1;
        }

        const BlifCircuit circuit = parseBlif(lines);
        // correspond the nodes with their output
        unordered_map<string, const BlifNode *> nodes;
        for (const BlifNode &node : circuit.nodes)
            nodes.emplace(node.output, &node);

        // build the truth table
        const size_t inputCount = circuit.inputs.size();
        const size_t rowCount = 1 << inputCount;

        truth_tables.resize(circuit.outputs.size());

        // loop through the input combinations
        for (size_t row = 0; row < rowCount; ++row)
        {
            unordered_map<string, bool> values;
            // input as a bit sequence
            for (size_t input = 0; input < inputCount; ++input)
            {
                const size_t bitPosition = inputCount - 1 - input;
                const bool value = ((row >> bitPosition) & 1U) != 0;
                values.emplace(circuit.inputs[input], value);
            }
            // evalaute each output and add to truth tables
            for (size_t output = 0; output < circuit.outputs.size(); ++output)
            {
                set<string> active;
                // recursion function that evaluates output backwards
                const bool value = evaluateBlifSignal(
                    circuit.outputs[output], nodes, values, active);
                truth_tables[output].push_back(value);
            }
        }
        // construct variables list from inputNames
        for (string input : circuit.inputs)
        {
            variables += input;
        }
        outputs = circuit.outputs;
        // cout << variables << endl;
        // for (bool v : truth_tables[0])
        // {
        //     cout << v << " ";
        // }
        // cout << endl;

        num_vars = variables.size();
        if (num_vars > 10)
        {
            std::cout << "Error: More than 10 variables present\n";
            return 1;
        }
        // implicant consisting of all ones
        max_minterm = (1 << variables.size()) - 1;
    }
    else
    {
        std::string expression;
        std::getline(std::cin, expression);
        // needs to be a letter and not already in variables
        for (char character : expression)
        {
            if (std::isalpha(character) && variables.find(character) == std::string::npos)
            {
                variables += character;
            }
        }
        std::sort(variables.begin(), variables.end());
        num_vars = variables.size();
        if (num_vars > 10)
        {
            std::cout << "Error: More than 10 variables present\n";
            return 1;
        }
        // implicant consisting of all ones
        max_minterm = (1 << variables.size()) - 1;

        // keep only the characters the parser reads, anything else like a space is ignored
        // also check that every opening parenthesis has a closing parenthesis
        std::string cleaned;
        int unclosed = 0;
        for (char character : expression)
        {
            if (std::isalpha(character) || character == '\'' || character == '+' || character == '(' || character == ')')
            {
                cleaned += character;
            }
            if (character == '(')
            {
                ++unclosed;
            }
            else if (character == ')')
            {
                if (unclosed == 0)
                {
                    std::cout << "Error: a ) has no matching (\n";
                    return 1;
                }
                --unclosed;
            }
        }
        if (unclosed > 0)
        {
            std::cout << "Error: a ( has no matching )\n";
            return 1;
        }
        truth_tables.resize(1);
        // build truth_table by evaluating the expression for every minterm
        std::vector<bool> truth_table(max_minterm + 1);
        for (int minterm = 0; minterm <= max_minterm; ++minterm)
        {
            size_t index = 0; // read from the first character
            truth_table[minterm] = evaluate_sum(cleaned, index, minterm);
        }

        truth_tables[0] = truth_table;
        outputs.resize(1);
        outputs[0] = "F";
    }
    std::vector<std::vector<std::string>> connections;
    for (size_t i = 0; i < truth_tables.size(); ++i)
    {

        cout << "processing output " << outputs[i] << endl;
        std::vector<bool> truth_table = truth_tables[i];
        std::vector<bool> inverse_truth_table(max_minterm + 1); // truth table of F'
        std::vector<int> on_set, off_set;
        // invert the truth table
        for (int minterm = 0; minterm <= max_minterm; ++minterm)
        {
            inverse_truth_table[minterm] = !truth_table[minterm];
            if (truth_table[minterm])
            {
                on_set.push_back(minterm);
            }
            else
            {
                off_set.push_back(minterm);
            }
        }

        // for (bool v : truth_table)
        // {
        //     cout << v << " ";
        // }
        // cout << endl;
        // cout << variables << endl;

        // the primes of F and F' each split once into the essential ones and the nonessential ones
        std::vector<Cube> prime_implicants = primes(truth_table), essential_primes, nonessential_primes;
        split(prime_implicants, essential_primes, nonessential_primes);
        std::vector<Cube> inverse_essential_primes, inverse_nonessential_primes;
        split(primes(inverse_truth_table), inverse_essential_primes, inverse_nonessential_primes);
        std::vector<Cube> minimized_sop = minimize(truth_table, essential_primes, nonessential_primes);

        std::cout << "Canonical SOP: F=" << sop(minterms(on_set)) << "\n";
        std::cout << "Canonical POS: F=" << pos(minterms(off_set)) << "\n";
        std::cout << "Inverse canonical SOP: F'=" << sop(minterms(off_set)) << "\n";
        std::cout << "Inverse canonical POS: F'=" << pos(minterms(on_set)) << "\n";
        std::cout << "Literal-minimized SOP: F=" << sop(minimized_sop) << "\n";
        std::cout << "Literal-minimized POS: F=" << pos(minimize(inverse_truth_table, inverse_essential_primes, inverse_nonessential_primes)) << "\n";
        std::cout << "Prime implicants: " << products(prime_implicants, ", ", "none") << "\n";
        std::cout << "Essential prime implicants: " << products(essential_primes, ", ", "none") << "\n";
        std::cout << "On-set minterms: m(" << numbers(on_set) << ")\n";
        std::cout << "Off-set maxterms: M(" << numbers(off_set) << ")\n";

        std::cout << "Estimated delay (gates on the slowest path): " << delay(minimized_sop) << "\n";

        connections.resize(outputs.size());
        if (write_file(outputs[i] + "_truth_table.csv", truth_table_csv(truth_table)))
        {
            std::cout << "Truth table: wrote truth_table.csv\n";
        }
        else
        {
            std::cout << "Truth table: could not write truth_table.csv\n";
        }

        // structural verilog of the minimized sop plus a quartus project for the cyclone v gx starter kit
        if (variables.size() > switch_pins.size())
        {
            std::cout << "Structural Verilog: not written, the board only has " << switch_pins.size() << " slide switches\n";
        }
        else if (write_file("sop_" + std::to_string(i) + ".v", verilog(minimized_sop, i, &connections[i])))
        {
            std::cout << "Structural Verilog: wrote sop.v, open sop.qpf in Quartus to put it on the Cyclone V GX board\n";
        }
        else
        {
            std::cout << "Structural Verilog: could not write sop.v, sop.qsf, sop.qpf\n";
        }
        cout << endl;
    }

    if (write_file("sop.v", verilog_top(connections)) && write_file("sop.qpf", "PROJECT_REVISION = \"sop\"\n"))
    {
        std::cout << "Structural Verilog: wrote sop.v, sop.qsf, sop.qpf\n";
    }
}
