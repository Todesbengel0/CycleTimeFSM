#pragma once
#include <string>

class Participant
{
public:
	explicit Participant(const unsigned short id, unsigned char* bytes, unsigned int count, bool isInput);
	Participant(const Participant& other);
	~Participant();
	void operator=(const Participant& other);

public:
	void ChangeBytes(unsigned char* bytes);

	int Cmp(unsigned char* bytes) const;

	int Cmp(const Participant& other) const;

	bool IsInput() const;

	std::string Print() const;

public:
	bool operator==(const Participant& other)
	{
		if (m_id != other.m_id
			|| m_byteCount != other.m_byteCount)
			return false;

		if (m_byteCount > 0 && m_bytes)
			if (other.Cmp(m_bytes) != 0)
				return false;

		return true;
	}
	bool operator!=(const Participant& other)
	{
		return !(*this == other);
	}

	bool operator()(const Participant& rhs) const
	{
		return Cmp(rhs) < 0;
	}

	bool operator<(const Participant& rhs) const
	{
		return Cmp(rhs) < 0;
	}

private:
	const unsigned short m_id;
	unsigned char* m_bytes;
	const unsigned int m_byteCount;
	const bool m_isInput;
};

