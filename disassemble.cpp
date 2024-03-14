#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <span>
#include <vector>

/* 
* mod (addresing mode) bits: 11 => register-register, otherwise memory is involved
* when memory involved, r/m field determines expression pattern (1 of 8 since 3 bits used for r/m)
* when memory involved, d field also determines if reg field is dest (read) or source (write)
*/

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

const char* jmpInstructions[] = {
	"jo", "jno", "jb", "jnb", "je", "jne", "jbe", "jnbe", "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jnle"
};

int DecodeModRegRmInstr(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t dw);
int DecodeArithmeticImmediateRm(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t sw);
int DecodeArithmeticImmediateAcc(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t w);

int main(int argc, char** argv)
{
	const char* binaryFilePath = argv[1];
	std::ifstream binaryFile(binaryFilePath, std::ios::binary);
	std::vector<char> fileContents;

	//get length of file
	binaryFile.seekg(0, binaryFile.end);
	size_t length = binaryFile.tellg();
	binaryFile.seekg(0, binaryFile.beg);

	//read file
	if (length > 0) {
		fileContents.resize(length);
		binaryFile.read(&fileContents[0], length);
	}

	std::span<std::uint8_t> binaryStream((std::uint8_t*)&fileContents[0], fileContents.size());

	std::ofstream asmFile("test.asm");
	asmFile << "bits 16\n";

	int instructionIndex = 0;
	for (unsigned int i = 0; i < binaryStream.size();)
	{
		std::uint8_t byte1 = binaryStream[i++];
		std::string asmInstr;
		if ((byte1 >> 2) == 0b100010) // r/m to/from register mov
		{
			asmInstr += "mov ";
			i = DecodeModRegRmInstr(binaryStream, i, asmInstr, byte1 & 0b00000011);
		}
		else if ((byte1 >> 4) == (std::uint8_t)0b1011) // immediate to register
		{
			asmInstr += "mov ";
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
		else if ((byte1 >> 2) == 0) 
		{
			asmInstr += "add ";
			i = DecodeModRegRmInstr(binaryStream, i, asmInstr, byte1 & 0b00000011);
		}
		else if ((byte1 >> 2) == 0b001010)
		{
			asmInstr += "sub ";
			i = DecodeModRegRmInstr(binaryStream, i, asmInstr, byte1 & 0b00000011);
		}
		else if ((byte1 >> 2) == 0b001110)
		{
			asmInstr += "cmp ";
			i = DecodeModRegRmInstr(binaryStream, i, asmInstr, byte1 & 0b00000011);
		}
		else if ((byte1 >> 2) == 0b100000)
		{
			std::uint8_t opcode = binaryStream[i];
			opcode = (opcode & 0b00111000) >> 3;
			if (opcode == 0)
			{
				asmInstr += "add ";
			}
			else if (opcode == 0b101)
			{
				asmInstr += "sub ";
			}
			else if (opcode == 0b111)
			{
				asmInstr += "cmp ";
			}

			i = DecodeArithmeticImmediateRm(binaryStream, i, asmInstr, byte1 & 0b00000011);
		}
		else if ((byte1 >> 1) == 0b0000010)
		{
			asmInstr += "add ";
			i = DecodeArithmeticImmediateAcc(binaryStream, i, asmInstr, byte1 & 0b1);
		}
		else if ((byte1 >> 1) == 0b0010110)
		{
			asmInstr += "sub ";
			i = DecodeArithmeticImmediateAcc(binaryStream, i, asmInstr, byte1 & 0b1);
		}
		else if ((byte1 >> 1) == 0b0011110)
		{
			asmInstr += "cmp ";
			i = DecodeArithmeticImmediateAcc(binaryStream, i, asmInstr, byte1 & 0b1);
		}
		else if ((byte1 >> 4) == 0b0111)
		{
			std::uint8_t opcode = byte1 & 0b1111;
			asmInstr += jmpInstructions[opcode];
			std::uint8_t byte2 = binaryStream[i++];
			std::int8_t offset = byte2;
			asmInstr += " ";
			asmInstr += std::to_string(offset);
		}
		else if ((byte1 >> 4) == 0b1110)
		{
			std::uint8_t opcode = byte1 & 0b1111;
			if (opcode == 0b0010)
			{
				asmInstr += "loop ";
			}
			else if (opcode == 0b0001)
			{
				asmInstr += "loopz ";
			}
			else if (opcode == 0)
			{
				asmInstr += "loopnz ";
			}
			else if (opcode == 0b0011)
			{
				asmInstr += "jcxz ";
			}
			std::uint8_t byte2 = binaryStream[i++];
			std::int8_t offset = byte2;
			asmInstr += std::to_string(offset);
		}
		else
		{
			std::cerr << "Instruction not supported\n";
			break;
		}
		asmFile << asmInstr << "\n";
		instructionIndex++;
	}
}

int DecodeModRegRmInstr(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t dw)
{
	bool regIsDest = dw & 0x0002; // d
	std::uint8_t byte2 = binaryStream[streamIdx++];
	unsigned int regW = ((byte2 >> 2) & 0x000E) | (dw & 0x0001); // 3 reg bits + w bit
	unsigned int rmW = ((byte2 << 1) & 0x000E) | (dw & 0x0001); // 3 r/m bits + w bit
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
			std::cerr << "Direct address mode not implemented\n";
		}
		else
		{
			addressOperand += addressExpressions[rm];
			if (mode >= 1)
			{
				std::uint16_t displacement = (std::uint8_t)binaryStream[streamIdx++];
				if (mode == 2)
				{
					displacement |= (std::uint16_t)binaryStream[streamIdx++] << 8;
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

	return streamIdx;
}

// TODO: handle direct addressing
int DecodeArithmeticImmediateRm(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t sw)
{
	bool signExtend = sw & 0b00000010;
	bool word = sw & 0b00000001;
	std::uint8_t byte2 = binaryStream[streamIdx++];
	std::uint8_t mode = byte2 >> 6;
	std::uint8_t rm = byte2 & 0b00000111;
	std::uint8_t rmW = (rm << 1) | word;
	std::string immediateStr;
	std::string destStr;
	if (mode == 0 || mode == 3)
	{
		if (mode == 0)
		{
			if (rm == 0b110)
			{
				std::uint16_t displacement = binaryStream[streamIdx++];
				displacement |= (std::uint16_t)binaryStream[streamIdx++] << 8;
				destStr = "word [";
				destStr += std::to_string(displacement) + "]";
			}
			else
			{
				destStr = word ? "word [" : "byte [";
				destStr += addressExpressions[rm];
				destStr += "]";
			}
		}
		else
		{
			destStr = registers[rmW];
		}

		std::uint16_t immediate = binaryStream[streamIdx++];
		if (sw == 0b01)
		{
			immediate |= (std::uint16_t)binaryStream[streamIdx++] << 8;
		}
		immediateStr = std::to_string(immediate);
	}
	else
	{
		std::uint16_t displacement = binaryStream[streamIdx++];
		if (mode == 2)
		{
			displacement |= (std::uint16_t)binaryStream[streamIdx++] << 8;
		}

		std::uint16_t immediate = binaryStream[streamIdx++];
		if (sw == 0b01)
		{
			immediate |= (std::uint16_t)binaryStream[streamIdx++] << 8;
		}
		immediateStr = std::to_string(immediate);

		destStr = word ? "word [" : "byte [";
		destStr += addressExpressions[rm];
		destStr += " + " + std::to_string(displacement) + "]";
	}

	asmInstr += destStr + ",";
	asmInstr += immediateStr;

	return streamIdx;
}

int DecodeArithmeticImmediateAcc(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t w)
{
	std::uint16_t immediate = binaryStream[streamIdx++];
	if (w)
	{
		immediate |= (std::uint16_t)binaryStream[streamIdx++] << 8;
		asmInstr += "ax, ";
	}
	else
	{
		asmInstr += "al, ";
	}

	asmInstr += std::to_string(immediate);

	return streamIdx;
}
