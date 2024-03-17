#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <span>
#include <vector>
#include <cassert>
#include <format>

/* 
* mod (addresing mode) bits: 11 => register-register, otherwise memory is involved
* when memory involved, r/m field determines expression pattern (1 of 8 since 3 bits used for r/m)
* when memory involved, d field also determines if reg field is dest (read) or source (write)
*/

const char* registerNames[] = {
	"al", "ax",
	"cl", "cx",
	"dl", "dx",
	"bl", "bx",
	"ah", "sp",
	"ch", "bp",
	"dh", "si",
	"bh", "di"
};

enum RegisterIndex
{
	a, c, d, b, sp, bp, si, di
};

struct RegisterOperand
{
	RegisterIndex idx;
	int mode; // 0 = l, 1 = h, 2 = x
	int nameIdx;
};

RegisterOperand registerOperands[] = {
	{RegisterIndex::a, 0, 0}, {RegisterIndex::a, 2, 1},
	{RegisterIndex::c, 0, 2}, {RegisterIndex::c, 2, 3},
	{RegisterIndex::d, 0, 4}, {RegisterIndex::d, 2, 5},
	{RegisterIndex::b, 0, 6}, {RegisterIndex::b, 2, 7},
	{RegisterIndex::a, 1, 8}, {RegisterIndex::sp, 2, 9},
	{RegisterIndex::b, 1, 10}, {RegisterIndex::bp, 2, 11},
	{RegisterIndex::c, 1, 12}, {RegisterIndex::si, 2, 13},
	{RegisterIndex::d, 1, 14}, {RegisterIndex::di, 2, 15},
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

std::uint16_t registers[8] = {};

struct MemoryOperand
{
	int addressExpressionIdx;
	std::uint16_t displacement;

	std::uint16_t Address()
	{
		assert(addressExpressionIdx < 8);
		if (addressExpressionIdx < 0)
		{
			return displacement;
		}
		switch (addressExpressionIdx)
		{
		case 0:
			return registers[RegisterIndex::b] + registers[RegisterIndex::si] + displacement;
		case 1:
			return registers[RegisterIndex::b] + registers[RegisterIndex::di] + displacement;
		case 2:
			return registers[RegisterIndex::bp] + registers[RegisterIndex::si] + displacement;
		case 3:
			return registers[RegisterIndex::bp] + registers[RegisterIndex::di] + displacement;
		case 4:
			return registers[RegisterIndex::si] + displacement;
		case 5:
			return registers[RegisterIndex::di] + displacement;
		case 6:
			return registers[RegisterIndex::bp] + displacement;
		case 7:
			return registers[RegisterIndex::b] + displacement;
		}
	}
};

struct Operand
{
	enum Type
	{
		Register, Immediate, Memory
	};

	Type type;

	union
	{
		RegisterOperand reg;
		std::uint16_t immediate;
		MemoryOperand mem;
	};
};

struct InstructionData
{
	Operand src;
	Operand dst;
	int streamIdx;
};

enum class ArithmeticOp {
	add, sub, cmp
};

bool SF;
bool ZF;

std::uint16_t memory[UINT16_MAX];

InstructionData DecodeModRegRmInstr(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t dw);
InstructionData DecodeArithmeticImmediateRm(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t sw);
InstructionData DecodeArithmeticImmediateAcc(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t w);

void SimArithmetic(const InstructionData& instrData, ArithmeticOp op, std::string& comment);

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
	for (unsigned int ip = 0; ip < binaryStream.size();)
	{
		unsigned int oldIP = ip;
		std::uint8_t byte1 = binaryStream[ip++];
		std::string asmInstr;
		if ((byte1 >> 2) == 0b100010) // r/m to/from register mov
		{
			asmInstr += "mov ";
			InstructionData instrData = DecodeModRegRmInstr(binaryStream, ip, asmInstr, byte1 & 0b00000011);
			ip = instrData.streamIdx;
			//assert(instrData.dst.type == Operand::Type::Register && instrData.src.type == Operand::Type::Register);
			std::uint16_t oldValue = registers[instrData.dst.reg.idx];
			std::uint16_t sourceValue;
			if (instrData.src.type == Operand::Type::Register)
			{
				sourceValue = registers[instrData.src.reg.idx];
			}
			else
			{
				assert(instrData.src.type == Operand::Type::Memory);
				std::uint16_t address = instrData.src.mem.Address();
				sourceValue = memory[address];
				sourceValue |= (std::uint16_t)memory[address + 1] << 8;
			}
			if (instrData.dst.type == Operand::Type::Register)
			{
				if (instrData.dst.reg.mode == 0)
				{
					std::uint16_t newValue = (oldValue & 0xFF00) | (sourceValue & 0x00FF);
					registers[instrData.dst.reg.idx] = newValue;
				}
				else if (instrData.dst.reg.mode == 1)
				{
					std::uint16_t newValue = (oldValue & 0x00FF) | (sourceValue & 0xFF00);
					registers[instrData.dst.reg.idx] = newValue;
				}
				else
				{
					registers[instrData.dst.reg.idx] = sourceValue;
				}

				asmInstr += std::format(" ; {}:{:x}->{:x}", registerNames[instrData.dst.reg.nameIdx], oldValue, registers[instrData.dst.reg.idx]);
			}
			else
			{
				assert(instrData.dst.type == Operand::Type::Memory);
				std::uint16_t address = instrData.dst.mem.Address();
				// assuming word-sized data will be written always
				memory[address] = sourceValue;
			}
		}
		else if ((byte1 >> 4) == (std::uint8_t)0b1011) // immediate to register
		{
			asmInstr += "mov ";
			std::uint8_t w = (byte1 & 0x08) >> 3;
			unsigned int registerIndex = ((byte1 << 1) & 0b00001110) | w;
			asmInstr += registerNames[registerIndex];
			asmInstr += ",";
			std::uint16_t immediate = (std::uint8_t)binaryStream[ip++];
			if (w)
			{
				immediate |= (std::uint16_t)binaryStream[ip++] << 8;
			}
			asmInstr += std::to_string(immediate);

			RegisterOperand reg = registerOperands[registerIndex];

			std::uint16_t oldValue = registers[reg.idx];

			if (reg.mode == 0)
			{
				std::uint16_t newValue = (oldValue & 0xFF00) | immediate;
				registers[reg.idx] = newValue;
			}
			else if (reg.mode == 1)
			{
				std::uint16_t newValue = (oldValue & 0x00FF) | (immediate << 8);
				registers[reg.idx] = newValue;
			}
			else
			{
				registers[reg.idx] = immediate;
			}

			asmInstr += std::format(" ; {}:{:x}->{:x}", registerNames[registerIndex], oldValue, registers[reg.idx]);
		}
		else if ((byte1 >> 1) == (std::uint8_t)0b1100011) // immediate to r/m
		{
			asmInstr += "mov ";
			InstructionData instrData = DecodeArithmeticImmediateRm(binaryStream, ip, asmInstr, byte1 & 0b1);
			ip = instrData.streamIdx;
			assert(instrData.src.type == Operand::Type::Immediate && (instrData.dst.type == Operand::Type::Register || instrData.dst.type == Operand::Type::Memory));
			// Always operating on word-sized data in hw so not going to bother checking 
			std::uint16_t address = instrData.dst.mem.Address();
			memory[address] = instrData.src.immediate;
			memory[address + 1] = instrData.src.immediate >> 8;
		}
		else if ((byte1 >> 2) == 0) 
		{
			asmInstr += "add ";
			InstructionData instrData = DecodeModRegRmInstr(binaryStream, ip, asmInstr, byte1 & 0b00000011);
			ip = instrData.streamIdx;
			assert(instrData.dst.type == Operand::Type::Register && instrData.src.type == Operand::Type::Register);
			std::string comment;
			SimArithmetic(instrData, ArithmeticOp::add, comment);
			asmInstr += comment;
		}
		else if ((byte1 >> 2) == 0b001010)
		{
			asmInstr += "sub ";
			InstructionData instrData = DecodeModRegRmInstr(binaryStream, ip, asmInstr, byte1 & 0b00000011);
			ip = instrData.streamIdx;
			assert(instrData.dst.type == Operand::Type::Register && instrData.src.type == Operand::Type::Register);
			std::string comment;
			SimArithmetic(instrData, ArithmeticOp::sub, comment);
			asmInstr += comment;
		}
		else if ((byte1 >> 2) == 0b001110)
		{
			asmInstr += "cmp ";
			InstructionData instrData = DecodeModRegRmInstr(binaryStream, ip, asmInstr, byte1 & 0b00000011);
			ip = instrData.streamIdx;
			assert(instrData.dst.type == Operand::Type::Register && instrData.src.type == Operand::Type::Register);
			std::string comment;
			SimArithmetic(instrData, ArithmeticOp::cmp, comment);
			asmInstr += comment;
		}
		else if ((byte1 >> 2) == 0b100000)
		{
			std::uint8_t opcode = binaryStream[ip];
			opcode = (opcode & 0b00111000) >> 3;
			std::string operandsStr;
			InstructionData instrData = DecodeArithmeticImmediateRm(binaryStream, ip, operandsStr, byte1 & 0b00000011);
			ip = instrData.streamIdx;

			std::string comment;
			if (opcode == 0)
			{
				asmInstr += "add ";
				SimArithmetic(instrData, ArithmeticOp::add, comment);
			}
			else if (opcode == 0b101)
			{
				asmInstr += "sub ";
				SimArithmetic(instrData, ArithmeticOp::sub, comment);
			}
			else if (opcode == 0b111)
			{
				asmInstr += "cmp ";
				SimArithmetic(instrData, ArithmeticOp::cmp, comment);
			}

			asmInstr += operandsStr + comment;
		}
		else if ((byte1 >> 1) == 0b0000010)
		{
			asmInstr += "add ";
			InstructionData instrData = DecodeArithmeticImmediateAcc(binaryStream, ip, asmInstr, byte1 & 0b1);
			ip = instrData.streamIdx;
			std::string comment;
			SimArithmetic(instrData, ArithmeticOp::add, comment);
			asmInstr += comment;
		}
		else if ((byte1 >> 1) == 0b0010110)
		{
			asmInstr += "sub ";
			InstructionData instrData = DecodeArithmeticImmediateAcc(binaryStream, ip, asmInstr, byte1 & 0b1);
			ip = instrData.streamIdx;
			std::string comment;
			SimArithmetic(instrData, ArithmeticOp::sub, comment);
			asmInstr += comment;
		}
		else if ((byte1 >> 1) == 0b0011110)
		{
			asmInstr += "cmp ";
			InstructionData instrData = DecodeArithmeticImmediateAcc(binaryStream, ip, asmInstr, byte1 & 0b1);
			ip = instrData.streamIdx;
			std::string comment;
			SimArithmetic(instrData, ArithmeticOp::cmp, comment);
			asmInstr += comment;
		}
		else if ((byte1 >> 4) == 0b0111)
		{
			std::uint8_t opcode = byte1 & 0b1111;
			asmInstr += jmpInstructions[opcode];
			std::uint8_t byte2 = binaryStream[ip++];
			std::int8_t offset = byte2;
			asmInstr += " ";
			asmInstr += std::to_string(offset);
			if (jmpInstructions[opcode] == "jne" && !ZF) // only simulating jne for the hw
			{
				ip += offset;
			}
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
			std::uint8_t byte2 = binaryStream[ip++];
			std::int8_t offset = byte2;
			asmInstr += std::to_string(offset);
		}
		else
		{
			std::cerr << "Instruction not supported\n";
			break;
		}

		asmInstr += std::format(" ; ip:0x{:x}->0x{:x}", oldIP, ip);
		asmFile << asmInstr << "\n";
		instructionIndex++;
	}

	std::cout << "Final register values:\n";
	for (int i = 0; i < 8; i++)
	{
		std::cout << std::format("{}:{:x}\n", registerNames[i * 2 + 1], registers[i]);
	}

	std::string flags;
	if (ZF) flags += "Z";
	if (SF) flags += "S";

	std::cout << "Flags: " << flags << '\n';
}

InstructionData DecodeModRegRmInstr(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t dw)
{
	InstructionData instrData;

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
			asmInstr += registerNames[regW];
			asmInstr += ",";
			asmInstr += registerNames[rmW];

			instrData.dst.reg = registerOperands[regW];
			instrData.dst.type = Operand::Type::Register;

			instrData.src.reg = registerOperands[rmW];
			instrData.src.type = Operand::Type::Register;
		}
		else
		{
			asmInstr += registerNames[rmW];
			asmInstr += ",";
			asmInstr += registerNames[regW];

			instrData.dst.reg = registerOperands[rmW];
			instrData.dst.type = Operand::Type::Register;

			instrData.src.reg = registerOperands[regW];
			instrData.src.type = Operand::Type::Register;
		}
	}
	else
	{
		instrData.src.reg = registerOperands[regW];
		instrData.src.type = Operand::Type::Register;

		std::string addressOperand = "[";
		bool directAddress = rm == 6 && mode == 0;
		if (directAddress)
		{
			std::uint16_t displacement = binaryStream[streamIdx++];
			displacement |= (std::uint16_t)binaryStream[streamIdx++] << 8;
			addressOperand += std::to_string(displacement);
			instrData.dst.type = Operand::Type::Memory;
			instrData.dst.mem.addressExpressionIdx = -1;
			instrData.dst.mem.displacement = displacement;
		}
		else
		{
			addressOperand += addressExpressions[rm];
			instrData.dst.type = Operand::Type::Memory;
			instrData.dst.mem.addressExpressionIdx = rm;
			instrData.dst.mem.displacement = 0;
			if (mode >= 1)
			{
				std::uint16_t displacement = (std::uint8_t)binaryStream[streamIdx++];
				if (mode == 2)
				{
					displacement |= (std::uint16_t)binaryStream[streamIdx++] << 8;
				}
				addressOperand += " + " + std::to_string(displacement);
				instrData.dst.mem.displacement = displacement;
			}
		}
		addressOperand += "]";

		if (regIsDest)
		{
			std::swap(instrData.dst, instrData.src);
			asmInstr += registerNames[regW];
			asmInstr += ",";
			asmInstr += addressOperand;
		}
		else
		{
			asmInstr += addressOperand;
			asmInstr += ",";
			asmInstr += registerNames[regW];
		}
	}

	instrData.streamIdx = streamIdx;
	return instrData;
}

// TODO: handle direct addressing
InstructionData DecodeArithmeticImmediateRm(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t sw)
{
	InstructionData instrData;
	bool word = sw & 0b00000001;
	std::uint8_t byte2 = binaryStream[streamIdx++];
	std::uint8_t mode = byte2 >> 6;
	std::uint8_t rm = byte2 & 0b00000111;
	std::uint8_t rmW = (rm << 1) | word;
	std::string immediateStr;
	std::string destStr;
	std::uint16_t immediate;
	// TODO: simulate memory access (return correct operands)
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
				instrData.dst.type = Operand::Type::Memory;
				instrData.dst.mem.addressExpressionIdx = -1;
				instrData.dst.mem.displacement = displacement;
			}
			else
			{
				destStr = word ? "word [" : "byte [";
				destStr += addressExpressions[rm];
				destStr += "]";
				instrData.dst.type = Operand::Type::Memory;
				instrData.dst.mem.addressExpressionIdx = rm;
				instrData.dst.mem.displacement = 0;
			}
		}
		else
		{
			instrData.dst.reg = registerOperands[rmW];
			instrData.dst.type = Operand::Type::Register;
			destStr = registerNames[rmW];
		}

		immediate = binaryStream[streamIdx++];
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

		immediate = binaryStream[streamIdx++];
		if (sw == 0b01)
		{
			immediate |= (std::uint16_t)binaryStream[streamIdx++] << 8;
		}
		immediateStr = std::to_string(immediate);

		destStr = word ? "word [" : "byte [";
		destStr += addressExpressions[rm];
		destStr += " + " + std::to_string(displacement) + "]";
		instrData.dst.type = Operand::Type::Memory;
		instrData.dst.mem.addressExpressionIdx = rm;
		instrData.dst.mem.displacement = displacement;
	}

	instrData.src.immediate = immediate;
	instrData.src.type = Operand::Type::Immediate;

	asmInstr += destStr + ",";
	asmInstr += immediateStr;

	instrData.streamIdx = streamIdx;

	return instrData;
}

InstructionData DecodeArithmeticImmediateAcc(const std::span<std::uint8_t>& binaryStream, int streamIdx, std::string& asmInstr, std::uint8_t w)
{
	InstructionData instrData;
	instrData.dst.type = Operand::Type::Register;
	std::uint16_t immediate = binaryStream[streamIdx++];
	if (w)
	{
		immediate |= (std::uint16_t)binaryStream[streamIdx++] << 8;
		asmInstr += "ax, ";
		instrData.dst.reg.idx = RegisterIndex::a;
		instrData.dst.reg.mode = 2;
	}
	else
	{
		asmInstr += "al, ";
		instrData.dst.reg.idx = RegisterIndex::a;
		instrData.dst.reg.mode = 0;
	}

	asmInstr += std::to_string(immediate);

	instrData.src.type = Operand::Type::Immediate;
	instrData.src.immediate = immediate;
	instrData.streamIdx = streamIdx;

	return instrData;
}

void SimArithmetic(const InstructionData& instrData, ArithmeticOp op, std::string& comment)
{
	bool oldZF = ZF;
	bool oldSF = SF;
	std::string oldFlagsStr;
	if (oldZF) oldFlagsStr += "Z";
	if (oldSF) oldFlagsStr += "S";

	// TODO: support memory dst ops
	assert(instrData.dst.type == Operand::Type::Register);
	std::uint16_t oldDstValue = registers[instrData.dst.reg.idx];
	std::uint16_t srcValue;
	switch (instrData.src.type)
	{
	case Operand::Type::Register:
		srcValue = registers[instrData.src.reg.idx];
		break;
	case Operand::Type::Immediate:
		srcValue = instrData.src.immediate;
		break;
	}

	std::uint16_t newValue;
	switch (op)
	{
	case ArithmeticOp::add:
		newValue = oldDstValue + srcValue;
		registers[instrData.dst.reg.idx] = newValue;
		break;
	case ArithmeticOp::sub:
		newValue = oldDstValue - srcValue;
		registers[instrData.dst.reg.idx] = newValue;
		break;
	case ArithmeticOp::cmp:
		newValue = oldDstValue - srcValue;
		break;
	}
	
	ZF = newValue == 0;
	SF = newValue >> 15;

	std::string newFlagsStr;
	if (ZF) newFlagsStr += "Z";
	if (SF) newFlagsStr += "S";

	if (oldDstValue != registers[instrData.dst.reg.idx])
	{
		comment = std::format(" ; {}:{:x}->{:x}", registerNames[instrData.dst.reg.nameIdx], oldDstValue, registers[instrData.dst.reg.idx]);
	}
	if (ZF != oldZF || SF != oldSF)
	{
		comment += std::format(" ; flags:{}->{}", oldFlagsStr, newFlagsStr);
	}
}
