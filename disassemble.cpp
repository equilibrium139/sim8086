#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

/* 
* mod (addresing mode) bits: 11 => register-register, otherwise memory is involved
* when memory involved, r/m field determines expression pattern (1 of 8 since 3 bits used for r/m)
* when memory involved, d field also determines if reg field is dest (read) or source (write)
*/

int main(int argc, char** argv)
{
	const char* binaryFilePath = argv[1];
	std::ifstream binaryFile(binaryFilePath, std::ios::binary);
	std::vector<char> binaryStream;

	//get length of file
	binaryFile.seekg(0, binaryFile.end);
	size_t length = binaryFile.tellg();
	binaryFile.seekg(0, binaryFile.beg);

	//read file
	if (length > 0) {
		binaryStream.resize(length);
		binaryFile.read(&binaryStream[0], length);
	}

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

	const char* addressExpressions[] = {
		"bx + si",
		"bx + di",
		"bp + si",
		"bp + di",
		"si",
		"di",
		"bp",
		"bx"
	};

	int instructionIndex = 0;
	for (unsigned int i = 0; i < binaryStream.size();)
	{
		std::uint8_t byte1 = binaryStream[i++];
		std::string asmInstr = "mov ";
		if ((byte1 >> 2) == (std::uint8_t)0b100010) // r/m to/from register mov
		{
			unsigned int regIsDest = byte1 & 0x0002; // d
			std::uint8_t byte2 = binaryStream[i++];
			unsigned int regW = ((byte2 >> 2) & 0x000E) | (byte1 & 0x0001); // 3 reg bits + w bit
			unsigned int rmW = ((byte2 << 1) & 0x000E) | (byte1 & 0x0001); // 3 r/m bits + w bit
			unsigned int rm = rmW >> 1;
			unsigned int mode = byte2 >> 6; // 2 mod bit
			if (mode == 3) // simple register-register move
			{
				if (regIsDest)
				{
					asmInstr += registers[regW];
					asmInstr += ",";
					asmInstr += registers[rmW];
				}
				else
				{
					asmInstr += registers[rmW];
					asmInstr += ",";
					asmInstr += registers[regW];
				}
			}
			else
			{
				std::string addressOperand = "[";
				bool directAddress = rm == 6 && mode == 0;
				if (directAddress)
				{

				}
				else
				{
					addressOperand += addressExpressions[rm];
					if (mode >= 1)
					{
						std::uint16_t displacement = (std::uint8_t)binaryStream[i++];
						if (mode == 2)
						{
							displacement |= (std::uint16_t)binaryStream[i++] << 8;
						}
						addressOperand += " + " + std::to_string(displacement);
					}
				}
				addressOperand += "]";

				if (regIsDest)
				{
					asmInstr += registers[regW];
					asmInstr += ",";
					asmInstr += addressOperand;
				}
				else
				{
					asmInstr += addressOperand;
					asmInstr += ",";
					asmInstr += registers[regW];
				}
			}
		}
		else if ((byte1 >> 4) == (std::uint8_t)0b1011) // immediate to register
		{
			std::uint8_t w = (byte1 & 0x08) >> 3;
			unsigned int registerIndex = ((byte1 << 1) & 0b00001110) | w;
			asmInstr += registers[registerIndex];
			asmInstr += ",";
			std::uint16_t immediate = (std::uint8_t)binaryStream[i++];
			if (w)
			{
				immediate |= (std::uint16_t)binaryStream[i++] << 8;
			}
			asmInstr += std::to_string(immediate);
		}

		asmFile << asmInstr << "\n";
		instructionIndex++;
	}
}