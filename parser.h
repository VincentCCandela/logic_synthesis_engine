#ifndef PARSER
#define PARSER

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <functional>
#include <limits>
using namespace std;

// represents a set of product terms where each term can be itself or its complement
struct ProductTerm
{
    map<char, bool> literals;
};

// a product term in BLIF format where input is a string of 1 and 0, and output is 1 or 0
struct BlifCube
{
    string inputs;
    bool output;
};

// represents a node in BLIF circuit that produces a output, cubes describe how the output is related to input
struct BlifNode
{
    string output;
    vector<string> inputs;
    vector<BlifCube> cubes;
};

// represents the entire BLIF circuit that contains all nodes
struct BlifCircuit
{
    string model;
    vector<string> inputs;
    vector<string> outputs;
    vector<BlifNode> nodes;
};

vector<string> splitWords(const string &line);
string trim(const string &value);
BlifCircuit parseBlif(const vector<string> &lines);
bool evaluateBlifSignal(const string &signal,
                        const unordered_map<string, const BlifNode *> &nodes,
                        unordered_map<string, bool> &values,
                        set<string> &active);

vector<string> generateBitstreams(
    size_t inputCount, size_t outputCount,
    const function<vector<bool>(const vector<bool> &)> &evaluateRow);
void exportTruthTable(const string &bitstream,
                      const vector<string> &inputNames,
                      const string &outputName,
                      const string &filePath);
void exportTruthTable(const vector<bool> &bitstream,
                      const vector<string> &inputNames,
                      const string &outputName,
                      const string &filePath);
vector<ProductTerm> parseSOP(const string &expr, set<char> &uniqueVars, string &outputName);
vector<string> generateSOPBitstream(
    const vector<vector<ProductTerm>> &outputTerms, const set<char> &varSet);
#endif