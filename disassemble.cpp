#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
	const char* binaryFilePath = argv[1];
	std::ifstream binaryFile(binaryFilePath, std::ios::binary);
	std::vector<char> binaryStream(std::istreambuf_iterator<char>(binaryFile), {});

	std::ofstream asmFile("test.asm");
	asmFile << "bits 16\n";

	const char* registers[] = {
		"al", "ax",
		"cl", "cx",
		"dl", "dx",
		"bl", "bx",
		"ah", "sp",
		"ch", "bp",
		"dh", "si",
		"bh", "di"
	};


	for (int i = 0; i < binaryStream.size();)
	{
		// register-register mov assumed
		char byte1 = binaryStream[i];
		char byte2 = binaryStream[i + 1];
		std::string asmInstr = "mov ";
		int regIndex = ((byte2 >> 2) & 0x000E) | (byte1 & 0x0001); // 3 reg bits + w bit
		int rmIndex = ((byte2 << 1) & 0x000E) | (byte1 & 0x0001); // 3 r/m bits + w bit
		int regIsDest = byte1 & 0x0002; // d
		if (regIsDest)
		{
			asmInstr += registers[regIndex];
			asmInstr += ",";
			asmInstr += registers[rmIndex];
		}
		else
		{
			asmInstr += registers[rmIndex];
			asmInstr += ",";
			asmInstr += registers[regIndex];
		}

		asmFile << asmInstr << "\n";

		i += 2;
	}
}