#include "FiniteStateMachine.h"
#include <stack>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

FiniteStateMachine::FiniteStateMachine(const std::string& filePath, bool combineStates /*= true*/, bool onlyOutput /*= false*/)
{
	std::weak_ptr<State> previousState;

	std::ifstream f(filePath);
	const json data = json::parse(f);
	auto& frames = data["frames"];

	for (auto& frame : frames)
	{
		auto timestamp = frame["timestamp"].get<uint64_t>();
		auto isInput = frame["input/output"].get<bool>();

		if (onlyOutput && isInput)
			continue;

		std::vector<Change> changes;

		for (auto& data : frame["data"])
		{
			unsigned int byteSize = data["byte"].size();
			changes.push_back(
				{
					data["participant"].get<unsigned short>(),
					byteSize
				});

			unsigned int i = 0;

			for (auto& byte : data["byte"])
				changes.back().bytes[i++] = byte.get<unsigned char>();
		}

		AddState(previousState, timestamp, isInput, changes, combineStates);
	}
	f.close();
}

FiniteStateMachine::~FiniteStateMachine()
{
}

void FiniteStateMachine::AddState(std::weak_ptr<State>& prevState, const uint64_t& timestamp, bool isInput, const std::vector<Change>& changes, bool combineStatesWithDuplicateValues)
{
	auto stateValues = StateValuesRegistry::GetStateValues(isInput, changes);
	if (stateValues.empty())
		return;
	if (!combineStatesWithDuplicateValues)
		stateValues.push_back(std::make_shared<Participant>(1000, new unsigned char[1] {(unsigned char)m_stateContainer.size()}, 1, false));

	auto state = std::make_shared<State>(stateValues, m_stateContainer.size());

	if (!m_startState)
	{
		m_stateContainer.insert(state);
		m_startState = state;
		prevState = m_startState;
		return;
	}

	// Search for the state values
	if (combineStatesWithDuplicateValues)
		state = *m_stateContainer.insert(state).first;
	else
		m_stateContainer.insert(state);

	auto& timestamps = prevState.lock()->transitions[state];

	if (timestamps.empty())
		++state->indegree;

	timestamps.insert(timestamp);

	prevState = state;
}

uint64_t FiniteStateMachine::GetStateCount()
{
	return m_stateContainer.size();
}

uint64_t FiniteStateMachine::CombineSequences()
{
	for (auto& currentState : m_stateContainer)
	{
		if (currentState != m_startState
			&& (currentState->transitions.empty()
			|| currentState->indegree == 1))
			continue;

		State::TransitionMap newTransitions;

		std::stack<State::TransitionMap*> transitionStack;
		transitionStack.push(&currentState->transitions);

		while (!transitionStack.empty())
		{
			auto currentTransitions = transitionStack.top();
			transitionStack.pop();

			for (auto& transition : *currentTransitions)
			{
				auto targetState = transition.first.lock();
				if (targetState->indegree == 1
					&& !targetState->transitions.empty()
					&& targetState != m_startState)
				{
					transitionStack.push(&targetState->transitions);
					continue;
				}

				auto retPair =
					newTransitions.insert(transition);

				// Check if insertion took place
				if (!retPair.second)
				{
					// If not, we need to reduce the reference count
					// And check if stateIt is 1 now
					if (--(*retPair.first).first.lock()->indegree == 1
						&& !targetState->transitions.empty()
						&& targetState != m_startState)
					{
						// If stateIt is, we add its transitions to the stack
						transitionStack.push(&(*retPair.first).first.lock()->transitions);
						// And erase stateIt from the stack
						newTransitions.erase(retPair.first);
					}
					else
					{
						// If stateIt is not, we need to insert the transition times
						auto& timestamps = (*retPair.first).second;
						timestamps.insert(
							transition.second.begin(),
							transition.second.end()
						);
					}
				}
			}
		}

		currentState->transitions = newTransitions;
	}

	// Delete States with only one reference
	return erase_if(m_stateContainer, [this](auto& state)
		{
			return state->indegree == 1 && !state->transitions.empty() && state != m_startState;
		}
	);
}

#ifndef COMBINED_STATES
void FiniteStateMachine::RemoveInputStates()
{
	std::set<std::shared_ptr<State>, State> newStates;
	auto currentState = m_startState;
	uint64_t currentTime = 0;

	auto newState = std::make_shared<State>( currentState->values, currentState->index, 0 );
	newStates.insert(newState);
	m_startState = newState;

	// Go through all the times
	while (!currentState->transitions.empty())
	{
		std::shared_ptr<State> nextState;

		// Find the next transition
		do {
			const uint64_t* closestTime = nullptr;
			for (auto& transition : currentState->transitions)
			{
				auto timeIt = std::find_if(
					transition.second.begin(),
					transition.second.end(),
					[currentTime](const uint64_t& timestamp)
					{
						return currentTime < timestamp;
					}
				);

				if (timeIt == transition.second.end())
					continue;

				if (closestTime)
					if (*timeIt > *closestTime)
						continue;

				nextState = transition.first.lock();
				closestTime = &*timeIt;
			}
			if (!nextState || !closestTime)
				break;

			currentTime = *closestTime;
			currentState = nextState;
		} while (currentState->values.front()->IsInput());


		auto stateIt = newStates.find(currentState);
		auto& stateValues = currentState->values;

		if (stateIt == newStates.end())
		{
			nextState = std::make_shared<State>( stateValues, currentState->index, 1 );
			newStates.insert(nextState);
		}
		else
		{
			nextState = *stateIt;
			// Not always correct
			++nextState->indegree;
		}

		newState->transitions[nextState].insert(currentTime);
		newState = nextState;
	}

	// delete the old states
	m_stateContainer.clear();

	m_stateContainer = newStates;
}
#endif

uint64_t FiniteStateMachine::CombineSCC()
{
	std::stack<std::pair<std::weak_ptr<State>, State::TransitionMap::iterator>> openStates;
	openStates.push({ m_startState, m_startState->transitions.begin() });

	while (!openStates.empty())
	{
		auto& currentElement = openStates.top();
		auto currentState = currentElement.first.lock();
		auto& currentTransitions = currentState->transitions;

		// Calculate the lowlink values
		if (!currentState->lowlink.lock())
			currentState->lowlink = currentState;
		
		while(currentElement.second != currentTransitions.end())
		{
			auto nextState = currentElement.second->first.lock();
			
			if (!nextState->lowlink.lock())
			{
				if (nextState->index > currentState->index)
				{
					openStates.push({ nextState, nextState->transitions.begin() });
					break;
				}

				if (nextState->index < currentState->lowlink.lock()->index)
					currentState->lowlink = nextState;
			}
			else if (nextState->lowlink.lock()->index < currentState->lowlink.lock()->index)
				currentState->lowlink = nextState->lowlink;

			++currentElement.second;
		}
		
		if (currentElement.second != currentTransitions.end())
			continue;

		openStates.pop();

		// The lowlink value is calculated
		if (currentState->lowlink.lock() != currentState)
		{
			for (auto& transition : currentTransitions)
			{
				auto adjacentState = transition.first.lock();
		
				if (adjacentState->lowlink.lock() != adjacentState)
					continue;
		
				auto retPair = currentState->lowlink.lock()->transitions.insert(transition);
		
				// Check if insertion took place
				if (!retPair.second)
				{
					// If not, we need to insert the transition times
					auto& timestamps = (*retPair.first).second;
					timestamps.insert(
						transition.second.begin(),
						transition.second.end()
					);
					--adjacentState->indegree;
				}
			}
		
			currentTransitions.clear();
		}
		else
		{
			std::erase_if(currentTransitions, [](auto& transition)
				{
					auto adjacentState = transition.first.lock();
					return adjacentState->lowlink.lock() != adjacentState;
				}
			);
		}
	}
 
	return std::erase_if(m_stateContainer, [](const auto& state)
		{
			return !state->lowlink.lock() || state->lowlink.lock() != state;
		}
	);
}

uint64_t FiniteStateMachine::MergeCircuits()
{
	auto cmp = [](std::weak_ptr<State> a, std::weak_ptr<State> b)
	{
		return a.lock()->index < b.lock()->index;
	};
	std::vector<std::weak_ptr<State>> sortedByNumber;
	std::copy(m_stateContainer.begin(), m_stateContainer.end(), back_inserter(sortedByNumber));
	std::sort(sortedByNumber.begin(), sortedByNumber.end(), cmp);

	auto end = sortedByNumber.end();
	for (auto it = sortedByNumber.begin(); it != end; ++it)
	{
		auto currentState = (*it).lock();

		// If the node is not reachable, stateIt can be ignored
		if (currentState->indegree == 0 && currentState != m_startState)
			continue;

		auto highestNumber = currentState->index;

		// Stack for states that need to be checked
		std::stack<std::shared_ptr<State>> mergeStack;
		std::shared_ptr<State> smallestState = nullptr;

		// Get all smaller states
		for (auto& transition : currentState->transitions)
		{
			auto adjacentState = transition.first.lock();

			// We only merge with smaller numbers
			if (adjacentState->index >= highestNumber)
				continue;

			// We need to merge with the end of the chain
			while (adjacentState->indegree == 0
				&& adjacentState != m_startState)
			{
				//mergeStack.push(adjacentState);
				adjacentState = adjacentState->transitions.begin()->first.lock();
			}

			mergeStack.push(adjacentState);

			if (!smallestState)
				smallestState = adjacentState;
			else if (smallestState->index > adjacentState->index)
				smallestState = adjacentState;
		}

		if (mergeStack.empty() || !smallestState)
			continue;
		
		const auto& smallestNumber = smallestState->index;

		// We go through the adjacent nodes of the smallest state
		// To check if there are nodes between the smallest and highest state
		// That we would need to add
		for (auto& transition : smallestState->transitions)
		{
			const auto& number = transition.first.lock()->index;
			if (number > smallestNumber
				&& number < highestNumber)
				mergeStack.push(transition.first.lock());
		}

		// We merge the highest and the smallest number
		for (auto& transition : currentState->transitions)
		{
			// If the transition ends in the lowest state
			// We add the timestamps
			if (transition.first.lock()->index == smallestNumber)
			{
				auto selfIt = smallestState->transitions.find(smallestState);
				if (selfIt != smallestState->transitions.end())
					--smallestState->indegree;
				auto& timestampArray = smallestState->transitions[smallestState];
				timestampArray.insert(
					transition.second.begin(),
					transition.second.end()
				);
			}
			// If stateIt ends in a higher state,
			// we add the whole transition
			else if (transition.first.lock()->index > highestNumber)
				smallestState->transitions.emplace(transition);
		}

		// We depreciate the highest state
		currentState->indegree = 0;
		currentState->transitions.clear();
		currentState->transitions[smallestState] = {};

		// We merge with the smallest state
		while (!mergeStack.empty())
		{
			currentState = mergeStack.top();
			mergeStack.pop();

			// We continue, if the state is the smallest state
			if (currentState->index == smallestNumber)
				continue;

			// If the state was already merged, we need to update
			// its merged node
			if (currentState->indegree == 0
				&& currentState->transitions.begin()->first.lock() != smallestState)
			{
				currentState->transitions.clear();
				currentState->transitions[smallestState] = {};
				continue;
			}

			// We go through the adjacent nodes
			for (auto& transition : currentState->transitions)
			{
				auto adjacentState = transition.first.lock();
				// If the adjacent State is greater than the highest number
				// We add the whole transition to the smallest state
				if (adjacentState->index > highestNumber)
					smallestState->transitions.emplace(transition);

				// Or push the state
				else
					mergeStack.push(adjacentState);
			}
			
			// We then depreciate the state
			currentState->indegree = 0;
			currentState->transitions.clear();
			currentState->transitions[smallestState] = {};
		}

		// Remove depreciated transitions from smallest state
		std::erase_if(smallestState->transitions, [](auto& transition)
			{
				return transition.first.lock()->indegree == 0;
			}
		);
	}

	// Delete depreciated states
	return std::erase_if(m_stateContainer, [this](auto& state)
		{
			return state->indegree == 0 && state != m_startState;
		}
	);
}

void FiniteStateMachine::RenumberStates()
{
	auto cmp = [](std::shared_ptr<State> a, std::shared_ptr<State> b)
	{
		return a->index < b->index;
	};
	std::vector<std::shared_ptr<State>> sortedByNumber;
	std::copy(m_stateContainer.begin(), m_stateContainer.end(), back_inserter(sortedByNumber));
	std::sort(sortedByNumber.begin(), sortedByNumber.end(), cmp);

	auto count = 0;
	for (auto& state : sortedByNumber)
		state->index = count++;
}

void FiniteStateMachine::RelativeTimes()
{
	auto currentState = m_startState;
	uint64_t lastTimestamp = 0;

	while (currentState)
	{
		std::shared_ptr<State> nextState;
		const uint64_t* nextTimestamp = nullptr;
		for (auto& [adjacent, timestamps] : currentState->transitions)
		{
			auto lowIt = std::find_if(timestamps.begin(), timestamps.end(),
				[&lastTimestamp](auto& timestamp)
				{
					return timestamp >= lastTimestamp;
				}
			);

			if (lowIt == timestamps.end())
				continue;

			if (!nextTimestamp || *lowIt < *nextTimestamp)
			{
				nextState = adjacent.lock();
				nextTimestamp = &(*lowIt);
			}
		}

		if (!nextTimestamp)
			return;

		auto relTime = *nextTimestamp - lastTimestamp;
		lastTimestamp = *nextTimestamp;

		currentState->transitions[nextState].erase(*nextTimestamp);
		currentState->transitions[nextState].insert(relTime);
		currentState = nextState;
	}
}

void FiniteStateMachine::CutToPart(const uint64_t& startIndex, const uint64_t& endIndex, bool ignoreBackEdges /*= false*/, uint64_t* tabooState /*= nullptr*/)
{
	auto findIt = std::find_if(m_stateContainer.begin(), m_stateContainer.end(),
		[startIndex](std::shared_ptr<State> state)
		{
			return state->index == startIndex;
		}
	);

	if (findIt == m_stateContainer.end())
		return;

	m_startState = *findIt;

	std::set<std::shared_ptr<State>, State> newContainer;

	std::stack<std::shared_ptr<State>> stack;
	stack.push(m_startState);

	while (!stack.empty())
	{
		auto currentState = stack.top();
		stack.pop();
		newContainer.insert(currentState);

		if (ignoreBackEdges)
			std::erase_if(currentState->transitions,
				[startIndex](const auto& transition)
				{
					return transition.first.lock()->index == startIndex;
				}
			);

		if (tabooState)
			std::erase_if(currentState->transitions,
				[tabooState](const auto& transition)
				{
					return transition.first.lock()->index == *tabooState;
				}
			);

		for (const auto& [adjacent, timestamps] : currentState->transitions)
			if (currentState->index != endIndex && newContainer.find(adjacent.lock()) == newContainer.end())
				stack.push(adjacent.lock());
	}

	auto backIt = std::find_if(newContainer.begin(), newContainer.end(),
		[endIndex](std::shared_ptr<State> state)
		{
			return state->index == endIndex;
		}
	);

	if (backIt == newContainer.end())
		return;

	std::erase_if((*backIt)->transitions, [&newContainer](auto& transition)
		{
			return newContainer.find(transition.first.lock()) == newContainer.end();
		}
	);

	m_stateContainer.clear();

	m_stateContainer = newContainer;
}

std::string FiniteStateMachine::GetStateValues(const uint64_t& stateNumber) const
{
	auto findIt = std::find_if(m_stateContainer.begin(), m_stateContainer.end(),
		[stateNumber](std::shared_ptr<State> state)
		{
			return state->index == stateNumber;
		}
	);

	if (findIt == m_stateContainer.end())
		return "";

	auto currentState = *findIt;

	std::string outString = "State " + std::to_string(currentState->index);

	outString +=
#ifndef COMBINED_STATES
		" (" + std::string(currentState->values.front()->IsInput() ? "Input" : "Output") + ")"
#endif
		":\n{";

	for(auto& value : currentState->values)
		outString += "\n\t" + value->Print();

	outString += "\n}\n";

	outString += "Input Transitions:\t" + std::to_string(currentState->indegree) + "\n";
	outString += "Output Transitions:\t" + std::to_string(currentState->transitions.size());

	return outString;
}

std::string FiniteStateMachine::GetTransitionTimes(const uint64_t& stateIndex) const
{
	auto findIt = std::find_if(m_stateContainer.begin(), m_stateContainer.end(),
		[stateIndex](std::shared_ptr<State> state)
		{
			return state->index == stateIndex;
		}
	);

	if (findIt == m_stateContainer.end())
		return "";

	auto currentState = *findIt;

	std::string outString = "State " + std::to_string(currentState->index) + "{";

	for (auto& transition : currentState->transitions)
	{
		outString += "\n\t" + std::to_string(transition.first.lock()->index) + ":\t{ ";
		for (auto& time : transition.second)
			outString += std::to_string((float)time * 1e-6) + "s ";
		outString += "}";
	}
	outString += "\n}";

	return outString;
}

std::string FiniteStateMachine::PrintTimes() const
{
	if (!m_startState)
		return "No States exist!";
	if (m_startState->transitions.empty())
		return "No Circuits exist!";

	auto currentState = m_startState;
	std::string outString = "";

	uint64_t lastTimestamp = 0;

	while (currentState)
	{
		outString += "State " + std::to_string(currentState->index);

		outString +=
#ifndef COMBINED_STATES
			" (" + std::string(currentState->values.front()->IsInput() ? "Input" : "Output") + ")"
#endif
			":\n{";

		for (auto& value : currentState->values)
			outString += "\n\t" + value->Print();

		outString += "\n}\n";

		float absTime = lastTimestamp * 1e-6f;
		outString += "Absolute Start Time: " + std::to_string(absTime) + "s\n";

		if (currentState->transitions.empty())
			return outString;

		for (auto& transition : currentState->transitions)
		{
			if (transition.first.lock() != currentState)
				continue;

			outString += "Cycle Times: ( ";

			auto end = std::prev(transition.second.end());

			for (auto it = transition.second.begin(); it != end; ++it)
			{
				auto currentTime = *it;
				float time = (currentTime - lastTimestamp) * 1e-6f;
				outString += std::to_string(time) + "s, ";
				lastTimestamp = currentTime;
			}
			
			auto currentTime = *transition.second.rbegin();
			float time = (currentTime - lastTimestamp) * 1e-6f;
			outString += std::to_string(time) + "s )\n";
			lastTimestamp = currentTime;

			break;
		}

		std::shared_ptr<State> nextState;

		for (auto& transition : currentState->transitions)
		{
			if (transition.first.lock() == currentState)
				continue;

			nextState = transition.first.lock();

			auto currentTime = *transition.second.begin();
			float time = (currentTime - lastTimestamp) * 1e-6f;
			outString += "Transition to next: " + std::to_string(time) + "s\n";
			lastTimestamp = currentTime;

			break;
		}
		if (nextState == nullptr)
			return outString;

		outString += "\n\n";

		currentState = nextState;
	}

	return outString;
}

std::string FiniteStateMachine::PrintTimeAutomata(
	const uint64_t& startState /*= 0*/,
	const uint64_t& finalState /*= 0*/,
	const std::string& statePrefix /*= "s"*/,
	unsigned short precision /*= 3*/) const
{
	auto cmp = [](std::shared_ptr<State> a, std::shared_ptr<State> b)
	{
		return a->index < b->index;
	};
	std::vector<std::shared_ptr<State>> sortedByNumber;
	std::copy(m_stateContainer.begin(), m_stateContainer.end(), back_inserter(sortedByNumber));
	std::sort(sortedByNumber.begin(), sortedByNumber.end(), cmp);

	bool printAll = startState >= finalState;

	std::set<std::string> stateStringVector;
	std::string stateString = "#states\n";
	std::string initialString = "#initial\n";
	std::string acceptingString = "#accepting\n";
	std::set<std::string> alphabetStringVector;
	std::string alphabetString = "#alphabet\n";
	std::string transitionString = "#transitions\n";

	initialString += statePrefix + std::to_string(startState) + '\n';

	uint64_t startTime = 0;
	
	auto currentState = m_startState;

	if (!printAll)
	{
		auto findIt = std::find_if(
			sortedByNumber.begin(),
			sortedByNumber.end(),
			[startState](std::shared_ptr<State> state)
			{
				return startState == state->index;
			}
		);

		if (findIt == sortedByNumber.end())
			return "";

		currentState = *findIt;

		for (auto& state : sortedByNumber)
		{
			if (state->index >= startState)
				break;
			
			auto it = state->transitions.find(currentState);
			if (state->transitions.end() != it)
			{
				uint64_t latestTime = *(*it).second.rbegin();
				if (latestTime > startTime)
					startTime = latestTime;
			}
			continue;
		}
	}

	while (printAll
		? !currentState->transitions.empty()
		: currentState->index != finalState && !currentState->transitions.empty())
	{
		std::string stateName = statePrefix + std::to_string(currentState->index);
		
		stateStringVector.insert(stateName);

		std::shared_ptr<State> nextState;
		const uint64_t* closestTime = nullptr;

		for (auto& transition : currentState->transitions)
		{
			auto timeIt = std::find_if(
				transition.second.begin(),
				transition.second.end(),
				[startTime](const uint64_t& timestamp)
				{
					return startTime < timestamp;
				}
			);

			if (timeIt == transition.second.end())
				continue;

			if (closestTime)
				if (*timeIt > *closestTime)
					continue;
			
			nextState = transition.first.lock();
			closestTime = &*timeIt;
		}

		if (!nextState)
			break;

		std::stringstream transStr;

		float timeDiff = (*closestTime - startTime) * 1e-6f;
		transStr.precision(precision);
		transStr << std::fixed << timeDiff << 's';

		alphabetStringVector.insert(transStr.str());

		transitionString += stateName + ":" + transStr.str() + ">" + statePrefix + std::to_string(nextState->index) + '\n';
	
		currentState = nextState;
		startTime = *closestTime;
	}

	if (printAll
		? currentState == sortedByNumber.back()
		: currentState->index == finalState)
		acceptingString += statePrefix + std::to_string(currentState->index) + '\n';

	stateStringVector.insert(statePrefix + std::to_string(currentState->index));

	for (auto& state : stateStringVector)
		stateString += state + '\n';
	for (auto& alphabet : alphabetStringVector)
		alphabetString += alphabet + '\n';

	return stateString + initialString + acceptingString + alphabetString + transitionString;

}

std::string FiniteStateMachine::PrintRightLinearGrammar(
	const uint64_t& startState /*= 0*/,
	const uint64_t& finalState /*= 0*/,
	const std::string& statePrefix /*= "s"*/,
	const std::string& transitionPrefix /*= ""*/,
	bool printProcentualDiff /*= false*/) const
{
	auto cmp = [](std::shared_ptr<State> a, std::shared_ptr<State> b)
	{
		return a->index < b->index;
	};
	std::set<std::shared_ptr<State>, decltype(cmp)> sortedByNumber;
	sortedByNumber.insert(m_stateContainer.begin(), m_stateContainer.end());
	
	bool printAll = startState >= finalState;
	uint64_t transitionCount = 0;

	auto startIt = printAll
		? sortedByNumber.begin()
		: std::find_if(sortedByNumber.begin(), sortedByNumber.end(),
			[startState](std::shared_ptr<State> state)
			{
				return state->index == startState;
			});

	std::string retString = "";

	for (; startIt != sortedByNumber.end(); ++startIt)
	{
		auto currentState = *startIt;

		if (!printAll && currentState->index >= finalState)
			break;

		retString += statePrefix + std::to_string(currentState->index) + " ->";

		for (const auto& transition : currentState->transitions)
		{
			auto adjacentState = transition.first.lock();

			if (printProcentualDiff)
			{
				auto size = transition.second.size();
				if (size > 1)
				{
					auto maxValue = *transition.second.rbegin();
					auto minValue = *transition.second.begin();
					auto sum = std::accumulate<std::set<uint64_t>::iterator, uint64_t>(transition.second.begin(), transition.second.end(), 0);
					double median = (double)sum / (double)size;
					double minFraction = median / minValue - 1.0f;
					double maxFraction = maxValue / median - 1.0f;
					std::stringstream fractionStr;
					fractionStr << std::fixed << std::setprecision(2) << std::max(minFraction, maxFraction) * 100.0f;
					retString += " " + fractionStr.str() + '%';
				}
				else
					retString += " 0%";
			}
			else
				retString += " " + transitionPrefix == ""
					? std::to_string(transition.second.size())
					: transitionPrefix + std::to_string(transitionCount++);

			if (printAll
				? !adjacentState->transitions.empty()
				: !adjacentState->transitions.empty()
				&& adjacentState->index != finalState)
				retString += " " + statePrefix + std::to_string(adjacentState->index);

			retString += " |";
		}

		retString.pop_back();
		retString.pop_back();
		retString += '\n';
	}

	return retString;
}

std::string FiniteStateMachine::PrintRegularAutomota(const uint64_t& startState /*= 0*/, const uint64_t& finalState /*= 0*/, const std::string& statePrefix /*= "s"*/, const std::string& transitionPrefix /*= "t" */,
	bool printProcentualDiff /*= false*/)
{
	auto cmp = [](std::shared_ptr<State> a, std::shared_ptr<State> b)
	{
		return a->index < b->index;
	};
	std::vector<std::shared_ptr<State>> sortedByNumber;
	std::copy(m_stateContainer.begin(), m_stateContainer.end(), back_inserter(sortedByNumber));
	std::sort(sortedByNumber.begin(), sortedByNumber.end(), cmp);
	auto end = sortedByNumber.end();

	bool printAll = startState >= finalState;

	std::set<std::string> stateStringVector;
	std::string stateString = "#states\n";
	std::string initialString = "#initial\n";
	std::string acceptingString = "#accepting\n";
	std::set<std::string> alphabetStringVector;
	std::string alphabetString = "#alphabet\n";
	std::string transitionString = "#transitions\n";

	initialString += statePrefix
		+ (printAll ? std::to_string(m_startState->index) : std::to_string(startState))
		+ '\n';

	auto currentStateIt = sortedByNumber.begin();

	if (!printAll)
	{
		auto findIt = std::find_if(
			currentStateIt,
			end,
			[startState](std::shared_ptr<State> state)
			{
				return startState == state->index;
			}
		);

		if (findIt == end)
			return "";

		currentStateIt = findIt;
	}

	std::size_t transitionCount = 0;

	for(; currentStateIt != end && (printAll || (*currentStateIt)->index <= finalState); ++currentStateIt)
	{
		auto& currentState = *currentStateIt;
		std::string stateName = statePrefix + std::to_string(currentState->index);

		stateStringVector.insert(stateName);

		if (currentState->transitions.empty()
			|| !printAll && currentState->index == finalState)
			acceptingString += stateName + '\n';

		for (const auto& transition : currentState->transitions)
		{
			auto nextNumber = transition.first.lock()->index;
			if (!printAll && (nextNumber < startState || nextNumber > finalState))
				continue;

			std::string transStr;
			
			if (printProcentualDiff)
			{
				auto size = transition.second.size();
				if (size > 1)
				{
					auto maxValue = *transition.second.rbegin();
					auto minValue = *transition.second.begin();
					auto sum = std::accumulate<std::set<uint64_t>::iterator, uint64_t>(transition.second.begin(), transition.second.end(), 0);
					double median = (double)sum / (double)size;
					double minFraction = median / minValue - 1.0f;
					double maxFraction = maxValue / median - 1.0f;
					std::stringstream fractionStr;
					fractionStr << std::fixed << std::setprecision(2) << std::max(minFraction, maxFraction) * 100.0f;
					transStr = fractionStr.str() + '%';
				}
				else
					transStr = "0%";
			}
			else
				transStr = transitionPrefix + std::to_string(transitionCount++);

			alphabetStringVector.insert(transStr);

			transitionString += stateName + ":" + transStr + ">" + statePrefix + std::to_string(nextNumber) + '\n';
		}
	}

	for (auto& state : stateStringVector)
		stateString += state + '\n';
	for (auto& alphabet : alphabetStringVector)
		alphabetString += alphabet + '\n';

	return stateString + initialString + acceptingString + alphabetString + transitionString;
}
