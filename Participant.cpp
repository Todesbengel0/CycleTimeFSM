#include "Participant.h"
#include <string.h>
#include <vector>

Participant::Participant(const unsigned short id, unsigned char* bytes, unsigned int count, bool isInput)
	: m_id(id)
	, m_byteCount(count)
	, m_bytes(bytes)
	, m_isInput(isInput)
{
}

Participant::Participant(const Participant& other)
	: m_id(other.m_id)
	, m_byteCount(other.m_byteCount)
	, m_isInput(other.m_isInput)
{
	if (m_byteCount > 0)
	{
		m_bytes = new unsigned char[m_byteCount];
		memcpy(m_bytes, other.m_bytes, m_byteCount);
	}
	else
		m_bytes = nullptr;
}

Participant::~Participant()
{
	delete[] m_bytes;
}

void Participant::operator=(const Participant& other)
{
	if (m_id != other.m_id || m_byteCount != other.m_byteCount)
		return;

	if (m_byteCount > 0)
	{
		delete[] m_bytes;
		m_bytes = new unsigned char[m_byteCount];
		memcpy(m_bytes, other.m_bytes, m_byteCount);
	}
}

void Participant::ChangeBytes(unsigned char* bytes)
{
	if (!m_bytes)
	{
		delete[] bytes;
		return;
	}
	
	delete[] m_bytes;
	m_bytes = bytes;
}

int Participant::Cmp(unsigned char* bytes) const
{
	if (!m_bytes || m_byteCount == 0)
		return 1;

	return memcmp(m_bytes, bytes, m_byteCount);
}

int Participant::Cmp(const Participant& other) const
{
	if (m_isInput != other.IsInput())
		return m_isInput ? -1 : 1;

	if (m_id < other.m_id) return -1;
	if (m_id > other.m_id) return 1;

	if (m_byteCount < other.m_byteCount) return -1;
	if (m_byteCount > other.m_byteCount) return 1;

	return other.Cmp(m_bytes);
}

bool Participant::IsInput() const
{
	return m_isInput;
}

std::string Participant::Print() const
{
	std::string outString = std::to_string(m_id) + ":\t( ";

	if (m_byteCount > 0 && m_bytes)
	{
		for (unsigned int i = 0; i < m_byteCount - 1; ++i)
			outString += std::to_string(m_bytes[i]) + ", ";

		outString += std::to_string(m_bytes[m_byteCount - 1]) + " ";
	}

	return outString + " )";
}
