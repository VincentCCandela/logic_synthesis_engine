#include "parser.h"

using namespace std;

// strips of leading and/or trailing spaces
string trim(const string &value)
{
    const size_t first = value.find_first_not_of(" \r\n\t");
    if (first == string::npos)
        return "";
    const size_t last = value.find_last_not_of(" \r\n\t");
    return value.substr(first, last - first + 1);
}

// split a line based on spaces into a vector
vector<string> splitWords(const string &line)
{
    istringstream stream(line);
    vector<string> words;
    string word;
    while (stream >> word)
        words.push_back(word);
    return words;
}

// construct a BLIF circuit from text
BlifCircuit parseBlif(const vector<string> &lines)
{
    BlifCircuit circuit;
    BlifNode *currentNode = nullptr;

    for (const string &physicalLine : lines)
    {
        // get the content before comments
        string line = physicalLine.substr(0, physicalLine.find('#'));
        line = trim(line);
        if (line.empty())
            continue;

        // check if the line starts with . to extract directives
        if (line[0] == '.')
        {
            currentNode = nullptr;
            const size_t firstSpace = line.find_first_of(" \t");
            const string directive = line.substr(0, firstSpace);
            const vector<string> args =
                firstSpace == string::npos ? vector<string>{}
                                           : splitWords(line.substr(firstSpace + 1));

            if (directive == ".model")
            {
                // get the name of the circuit
                circuit.model = args[0];
            }
            else if (directive == ".inputs" || directive == ".outputs")
            {
                // get overall inputs and outputs
                vector<string> &signals =
                    directive == ".inputs" ? circuit.inputs : circuit.outputs;
                signals.insert(signals.end(), args.begin(), args.end());
            }
            else if (directive == ".names")
            {
                // get the inputs/outputs of each node
                BlifNode node;
                node.output = args.back();
                node.inputs.assign(args.begin(), args.end() - 1);
                circuit.nodes.push_back(move(node));
                currentNode = &circuit.nodes.back();
            }
            continue;
        }

        // if the current line describes an entry in the truth table
        const vector<string> fields = splitWords(line);
        const size_t faninCount = currentNode->inputs.size();

        BlifCube cube;
        if (faninCount == 0)
        {
            cube.inputs = "";
            cube.output = fields[0] == "1";
        }
        else
        {
            cube.inputs = fields[0];
            const string outputValue = fields.size() == 1 ? "1" : fields[1];
            cube.output = outputValue == "1";
        }
        currentNode->cubes.push_back(move(cube));
    }

    return circuit;
}

// find the value of the given signal
bool evaluateBlifSignal(const string &signal,
                        const unordered_map<string, const BlifNode *> &nodes,
                        unordered_map<string, bool> &values,
                        set<string> &active)
{
    // if already cached in values, return it
    const auto knownValue = values.find(signal);
    if (knownValue != values.end())
        return knownValue->second;

    // find if the target output signal exists in the node set nodes
    const auto nodeIt = nodes.find(signal);
    if (nodeIt == nodes.end())
        throw runtime_error("undefined signal '" + signal + "'");
    // active is the set of evaluated signals, used to detect cycles
    if (!active.insert(signal).second)
        throw runtime_error("cycle detected at signal '" + signal + "'");

    // get the BLIF node definition in the node set nodes for the target output
    const BlifNode &node = *nodeIt->second;
    vector<bool> inputValues;
    inputValues.reserve(node.inputs.size());
    // recursion to evaluate all the inputs to the target node
    for (const string &input : node.inputs)
        inputValues.push_back(evaluateBlifSignal(input, nodes, values, active));

    // check if there is one cube in all cubes that gives a 1
    bool hasOneCube = false;
    for (const BlifCube &cube : node.cubes)
        hasOneCube = hasOneCube || cube.output;
    bool result = !node.cubes.empty() && !hasOneCube;
    for (const BlifCube &cube : node.cubes)
    {
        // check if the current row matches the evaluated input
        bool cubeMatches = true;
        for (size_t i = 0; i < cube.inputs.size(); ++i)
        {
            if (cube.inputs[i] != '-' &&
                (cube.inputs[i] == '1') != inputValues[i])
            {
                cubeMatches = false;
                break;
            }
        }
        if (!cubeMatches)
            continue;
        // if the row matches the input, assign the result to the row's output
        result = cube.output;
    }

    active.erase(signal);
    values.emplace(signal, result);
    return result;
}
