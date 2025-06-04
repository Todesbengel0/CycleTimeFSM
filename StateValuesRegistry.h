#pragma once
#include "Participant.h"
#include <set>
#include <memory>
#include <vector>
#include <initializer_list>

//#define COUNT_DUPLICATES
//#define COMBINED_STATES

using participant_ptr_t = std::shared_ptr<const Participant>;

struct SmartCmp {
	bool operator()(participant_ptr_t lhs, participant_ptr_t rhs) const
	{
		return *lhs < *rhs;
	}
};

struct Change
{
	unsigned short participantId;
	unsigned int byteCount;
	unsigned char* bytes = new unsigned char[byteCount];
};

struct Registry
{
	Participant** current = nullptr;
	unsigned short participantCount = 0;
#ifdef COUNT_DUPLICATES
	std::vector<std::vector<participant_ptr_t>> states;
#endif
	std::set<participant_ptr_t, SmartCmp>* values = nullptr;
};

/// <summary>
/// Singleton Class for keeping track of all possible state values
/// </summary>
class StateValuesRegistry
{
protected:
	StateValuesRegistry(Participant** participants, unsigned short participantCount, bool isInput);
	void InitRegistry(Participant** participants, unsigned short participantCount, bool isInput);
	~StateValuesRegistry();
	static StateValuesRegistry* s_instance;

	std::vector<participant_ptr_t> FindCurrentValues(bool isInput);

public:
	StateValuesRegistry(StateValuesRegistry& other) = delete;
	void operator=(const StateValuesRegistry&) = delete;

	static std::vector<participant_ptr_t> GetStateValues(bool isInput, const std::vector<Change>& changes);
	
#ifdef COUNT_DUPLICATES
	static unsigned int GetNumberOfDuplicates();
	static std::size_t GetUniqueInputStates();
	static std::size_t GetUniqueOutputStates();
#endif
	
	static unsigned short GetInputParticipantCount();
	static unsigned short GetOutputParticipantCount();

protected:
	Registry m_inputRegistry;
	Registry m_outputRegistry;
#ifdef COUNT_DUPLICATES
	unsigned int m_duplicateStates;
#endif
};