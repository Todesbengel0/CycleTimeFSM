#include "StateValuesRegistry.h"

StateValuesRegistry::StateValuesRegistry(Participant** participants, unsigned short participantCount, bool isInput)
#ifdef COUNT_DUPLICATES
	: m_duplicateStates(0)
#endif
{
	InitRegistry(participants, participantCount, isInput);
}

void StateValuesRegistry::InitRegistry(Participant** participants, unsigned short participantCount, bool isInput)
{
	auto& registry = isInput ? m_inputRegistry : m_outputRegistry;
	registry.participantCount = participantCount;
	registry.values = new std::set<participant_ptr_t, SmartCmp>[participantCount];
	registry.current = participants;
}

StateValuesRegistry::~StateValuesRegistry()
{
	for (unsigned short i = 0; 0 < m_inputRegistry.participantCount; ++i)
		delete m_inputRegistry.current[i];

	delete[] m_inputRegistry.values;

	for (unsigned short i = 0; 0 < m_outputRegistry.participantCount; ++i)
		delete m_outputRegistry.current[i];

	delete[] m_outputRegistry.values;
}

StateValuesRegistry* StateValuesRegistry::s_instance = nullptr;

std::vector<participant_ptr_t> StateValuesRegistry::FindCurrentValues(bool isInput)
{
	auto& registry = isInput ? m_inputRegistry : m_outputRegistry;

#ifdef COUNT_DUPLICATES
	for (auto& values : registry.states)
	{
		bool isEqual = true;
		for (unsigned short i = 0; i < registry.participantCount; ++i)
		{
			if ((*registry.current[i]) != (*values[i]))
			{
				isEqual = false;
				break;
			}
		}

		if (isEqual)
		{
			++m_duplicateStates;
			return values;
		}
	}
#endif

	std::vector<participant_ptr_t> newState;

	for (unsigned short i = 0; i < registry.participantCount; ++i)
	{
		auto currentValue = std::make_shared<const Participant>(*registry.current[i]);

		auto findIt = registry.values[i].find(currentValue);

		if (findIt != registry.values[i].end())
		{
			newState.push_back(*findIt);
			continue;
		}
		
		registry.values[i].insert(currentValue);
		newState.push_back(currentValue);
	}

#ifdef COUNT_DUPLICATES
	registry.states.push_back(newState);
#endif

	return newState;
}

std::vector<participant_ptr_t> StateValuesRegistry::GetStateValues(bool isInput, const std::vector<Change>& changes)
{
	bool createNew = s_instance ?
		(isInput ? s_instance->m_inputRegistry.participantCount : s_instance->m_outputRegistry.participantCount) == 0
		: true;

	if (createNew)
	{
		auto count = static_cast<uint16_t>(changes.size());
		Participant** participants = createNew ? new Participant*[count] : nullptr;

		for (auto& change : changes)
		{
			uint16_t idx = 0 - change.participantId;

			if (idx >= count)
			{
				auto oldArray = participants;
				participants = new Participant * [idx + 1];
				memcpy(participants, oldArray, count);
				count = idx + 1;
				delete[] oldArray;
			}

			participants[idx] = new Participant(change.participantId, change.bytes, change.byteCount, isInput);
		}

		if (!s_instance)
			s_instance = new StateValuesRegistry(participants, count, isInput);
		else
			s_instance->InitRegistry(participants, count, isInput);

#ifdef COMBINED_STATES
		if (s_instance->m_inputRegistry.participantCount == 0 || s_instance->m_outputRegistry.participantCount == 0)
			return {};

		auto values = s_instance->FindCurrentValues(true);
		auto outputValues = s_instance->FindCurrentValues(false);
		values.insert(values.end(), outputValues.begin(), outputValues.end());
		return values;

#else
		return s_instance->FindCurrentValues(isInput);
#endif
	}

	for (auto& change : changes)
	{
		auto idx = static_cast<uint16_t>(0 - change.participantId);
		
		(isInput ? s_instance->m_inputRegistry : s_instance->m_outputRegistry).current[idx]
			->ChangeBytes(change.bytes);
	}

#ifdef COMBINED_STATES
	if (s_instance->m_inputRegistry.participantCount == 0 || s_instance->m_outputRegistry.participantCount == 0)
		return {};

	auto values = s_instance->FindCurrentValues(true);
	auto outputValues = s_instance->FindCurrentValues(false);
	values.insert(values.end(), outputValues.begin(), outputValues.end());
	return values;

#else
	return s_instance->FindCurrentValues(isInput);
#endif
}

#ifdef COUNT_DUPLICATES
unsigned int StateValuesRegistry::GetNumberOfDuplicates()
{
	if (!s_instance)
		return 0;
	return s_instance->m_duplicateStates;
}

std::size_t StateValuesRegistry::GetUniqueInputStates()
{
	if (!s_instance)
		return 0;
	return s_instance->m_inputRegistry.states.size();
}

std::size_t StateValuesRegistry::GetUniqueOutputStates()
{
	if (!s_instance)
		return 0;
	return s_instance->m_outputRegistry.states.size();
}
#endif

unsigned short StateValuesRegistry::GetInputParticipantCount()
{
	if (!s_instance)
		return 0;
	return s_instance->m_inputRegistry.participantCount;
}

unsigned short StateValuesRegistry::GetOutputParticipantCount()
{
	if (!s_instance)
		return 0;
	return s_instance->m_outputRegistry.participantCount;
}
