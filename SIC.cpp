#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <iomanip>
using namespace std;

unordered_map<string,string> OPTAB = {
    {"ADD","18"},{"AND","40"},{"COMP","28"},{"DIV","24"},{"J","3C"},{"JEQ","30"},{"JGT","34"},{"JLT","38"},{"JSUB","48"},{"LDA","00"},
    {"LDB","68"},{"LDCH","50"},{"LDF","70"},{"LDL","08"},{"LDS","6C"},{"LDT","74"},{"LDX","04"},{"MUL","20"},{"OR","44"},{"RD","D8"},
    {"RSUB","4C"},{"STA","0C"},{"STB","78"},{"STCH","54"},{"STF","80"},{"STL","14"},{"STS","7C"},{"STSW","E8"},{"STT","84"},{"STX","10"},
    {"SUB","1C"},{"TD","E0"},{"TIX","2C"},{"WD","DC"}
};

int proglength=0,startlen=0;
unordered_map<string, int> SYMTAB;

// Helper function to split a line into 3 parts
vector<string> splitLine(string &line)
{
    int l = line.length();
    string temp = "";
    int i = 0;
    vector<string> operandtable;

    // leading spaces
    while (i < l && line[i] == ' ') i++;

    // First part: Attempt to read a label or mnemonic
    while (i < l && line[i] != ' ') {
        temp += line[i];
        i++;
    }
    operandtable.push_back(temp); // First can be a label or mnemonic
    temp = "";

    // Skip spaces
    while (i < l && line[i] == ' ') i++;

    // Second part: Check if the first mnemonic
    if (OPTAB.find(operandtable[0]) != OPTAB.end() || operandtable[0] == "START" || operandtable[0] == "END" ||
        operandtable[0] == "WORD" || operandtable[0] == "RESW" || operandtable[0] == "RESB" || operandtable[0] == "BYTE") {
        // No label; this is actually the mnemonic
        operandtable.insert(operandtable.begin(), "-");  // Insert "-" for missing label
    } else {
        // We assume the first token was a label; read the next token for mnemonic
        while (i < l && line[i] != ' ') {
            temp += line[i];
            i++;
        }
        operandtable.push_back(temp);  // Add mnemonic
        temp = "";
    }

    // Skip spaces
    while (i < l && line[i] == ' ') i++;

    // Third part: Operand
    while (i < l && line[i] != ' ') {
        temp += line[i];
        i++;
    }
    operandtable.push_back(temp.empty() ? "-" : temp);  // Add operand or "-" if empty

    // Ensure vector always has three elements (label, mnemonic, operand)
    while (operandtable.size() < 3) {
        operandtable.push_back("-");
    }

    return operandtable;
}




// Convert integer to hex string
string intToHex(int value, int width) {
    stringstream stream;
    stream << setw(width) << setfill('0') << hex << value;
    return stream.str();
}



// Pass 1: Build Symbol Table and Calculate Addresses
int pass1(string &inputFile) {
    ifstream infile(inputFile);
    string line;
    int LOCCTR = 0;
    bool startFound = false;
    while (getline(infile, line)) {
        auto tokens = splitLine(line);
        if (tokens.empty()) continue;

        if (tokens[1] == "START") {
            LOCCTR = stoi(tokens[2], nullptr, 16);
            startlen=LOCCTR;
            startFound = true;
            cout << "Starting address: " << intToHex(LOCCTR, 4) << endl;
            continue;
        }
        if(tokens[1] == "END") {
            proglength = LOCCTR;
            break;
        }

        // Check if we have a label and add it to SYMTAB
        if (tokens[0] != "-") {  // Label field
            if (SYMTAB.find(tokens[0]) == SYMTAB.end()) {
                SYMTAB[tokens[0]] = LOCCTR;
                cout << "Label added to SYMTAB: " << tokens[0] << " at address " << intToHex(LOCCTR, 4) << endl;
            } else {
                cerr << "Error: Duplicate symbol " << tokens[0] << " at address " << intToHex(LOCCTR, 4) << endl;
                return -1;
            }
        }

        if (OPTAB.find(tokens[1]) != OPTAB.end()) {
            LOCCTR += 3;  // 3 bytes per instruction
        } else if (tokens[1] == "WORD") {
            LOCCTR += 3;
        } else if (tokens[1] == "RESW") {
            LOCCTR += 3 * stoi(tokens[2]);
        } else if (tokens[1] == "RESB") {
            LOCCTR += stoi(tokens[2]);
        } else if (tokens[1] == "BYTE") {
            if (tokens[2][0] == 'C') {  // e.g., BYTE C'EOF'
                LOCCTR += tokens[2].length() - 3;
            } else if (tokens[2][0] == 'X') {  // e.g., BYTE X'F1'
                LOCCTR += (tokens[2].length() - 3) / 2;
            }
        } else {
            cerr << "Error: Invalid operation code " << tokens[1] << " at address " << intToHex(LOCCTR, 4) << endl;
            return -1;
        }
    }
    infile.close();
    return startFound ? LOCCTR : -1;
}


// Pass 2: Generate Object Code
// Pass 2: Generate Object Code with Indexed Addressing
void pass2(string &inputFile, string &outputFile) {
    ifstream infile(inputFile);
    ofstream outfile(outputFile);
    string line;
    int LOCCTR = 0;
    string currentObjCode = "";  // To accumulate the object code for one T record
    int currentStartAddress = 0;  // Start address of the current T record
    int currentLength = 0;  // Length of the current T record

    while (getline(infile, line)) {
        auto tokens = splitLine(line);
        if (tokens.empty()) continue;

        if (tokens[1] == "START") {
            LOCCTR = stoi(tokens[2], nullptr, 16);
            outfile << "H^" << tokens[0] << "^" << intToHex(LOCCTR, 6) << "^" << intToHex(proglength-LOCCTR, 6) << endl;
            continue;
        }

        string objCode = "";

        // Process instructions with opcodes
        if (OPTAB.find(tokens[1]) != OPTAB.end()) {

            if (currentLength == 0) {
                currentStartAddress = LOCCTR;
            }

            objCode = OPTAB[tokens[1]];  // Get the opcode
            string operandAddress = "0000";  // Default operand address in case of errors

            // Check if operand exists and handle indexed addressing
            if (tokens[2] != "-") {
                bool indexed = false;
                string operand = tokens[2];

                // Check for indexed addressing (e.g., LABEL,X)
                if (operand.size() > 2 && operand.substr(operand.size() - 2) == ",X") {
                    indexed = true;
                    operand = operand.substr(0, operand.size() - 2);  // Remove ",X" to get label
                }

                // Look up the symbol in SYMTAB
                if (SYMTAB.find(operand) != SYMTAB.end()) {
                    int address = SYMTAB[operand];
                    
                    // Add 0x8000 to the address for indexed addressing
                    if (indexed) {
                        address += 0x8000;  // Set the indexed addressing bit
                    }

                    // Convert the final address to hex (4-digit format)
                    operandAddress = intToHex(address, 4);
                } else {
                    cerr << "Error: Undefined symbol " << operand << endl;
                }
            }

            // Final object code: opcode + last 4 hex digits of operand address
            objCode += operandAddress;
            
            // Accumulate the object code for the current text record
            currentObjCode += objCode+" ";
            currentLength += objCode.length() / 2;  // Each byte is represented by 2 hex characters

            // Check if we need to write the T record (if we've reached 30 bytes)
            if (currentLength >= 30) {  // 60 hex characters = 30 bytes
                outfile << "T^" << intToHex(currentStartAddress, 6) << "^" 
                        << intToHex(currentLength, 2) << "^" << currentObjCode << endl;
                currentObjCode = "";  // Reset for the next T record
                currentLength = 0;  // Reset length
            }

            LOCCTR += 3;  // Each instruction is 3 bytes
        } 
        // Process "WORD" directive
        else if (tokens[1] == "WORD") {
            objCode = intToHex(stoi(tokens[2]), 6);  // Convert to 6-digit hex
            currentObjCode += objCode+" ";
            currentLength += objCode.length() / 2;
            if (currentLength >= 30) {  // Check if we need to write the T record
                outfile << "T^" << intToHex(currentStartAddress, 6) << "^"
                        << intToHex(currentLength, 2) << "^" << currentObjCode << endl;
                currentObjCode = "";
                currentLength = 0;
                currentStartAddress = LOCCTR + 3;
            }
            LOCCTR += 3;
        } 
        // Process "BYTE" directive
        else if (tokens[1] == "BYTE") {
            if (tokens[2][0] == 'C') {  // Character constant (C'EOF')
                objCode = "";
                for (size_t i = 2; i < tokens[2].size() - 1; ++i) {
                    objCode += intToHex((int)tokens[2][i], 2);
                }
            } else if (tokens[2][0] == 'X') {  // Hex constant (X'F1')
                objCode = tokens[2].substr(2, tokens[2].size() - 3);  // Remove X' and ' for hex values
            }
            currentObjCode += objCode+" ";
            currentLength += objCode.length();
            if (currentLength >= 60) {  // Check if we need to write the T record
                outfile << "T^" << intToHex(currentStartAddress, 6) << "^"
                        << intToHex(currentLength, 2) << "^" << currentObjCode << endl;
                currentObjCode = "";
                currentLength = 0;
                currentStartAddress = LOCCTR + objCode.length() / 2;
            }
            LOCCTR += objCode.length() / 2;  // Update LOCCTR for BYTE length
        }
        // Handle RESW and RESB (skip object code generation but update LOCCTR)
        else if (tokens[1] == "RESW" || tokens[1] == "RESB") {
            if (!currentObjCode.empty()) {
                outfile << "T^" << intToHex(currentStartAddress, 6) << "^"
                        << intToHex(currentLength, 2) << "^" << currentObjCode << endl;
                currentObjCode = "";  // Reset object code
                currentLength = 0;  // Reset length
            }

            if (tokens[1] == "RESW") {
                LOCCTR += 3 * stoi(tokens[2]);
                currentStartAddress = LOCCTR;
            } else if (tokens[1] == "RESB") {
                LOCCTR += stoi(tokens[2]);
                currentStartAddress = LOCCTR;
            }
            continue;
        }
    }

    // Write the last T record if any object code is left
    if (!currentObjCode.empty()) {
        outfile << "T^" << intToHex(currentStartAddress, 6) << "^" 
                << intToHex(currentLength, 2) << "^" << currentObjCode << endl;
    }

    // Write the "E" record (entry point address)
    outfile << "E^" << intToHex(startlen, 6) << endl;

    infile.close();
    outfile.close();
}



int main() {
    string inputFile = "Input.txt";
    string outputFile = "program.obj";

    int programLength = pass1(inputFile);
    if (programLength == -1) {
        cerr << "Error in Pass 1" << endl;
        return 1;
    }

    pass2(inputFile, outputFile);
    cout << "Assembly completed. Object code written to " << outputFile << endl;
    return 0;
} 