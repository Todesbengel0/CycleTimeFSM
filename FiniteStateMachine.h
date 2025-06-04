#pragma once
#include "StateValuesRegistry.h"
#include <iostream>
#include <ostream>
#include <sstream>
#include <fstream>
#include <map>

struct State;

struct State
{
	using TransitionMap = std::map<std::weak_ptr<State>, std::set<uint64_t>, std::owner_less<std::weak_ptr<State>>>;

	std::vector<participant_ptr_t> values;
	uint64_t index = 0;
	uint64_t indegree = 0;
	TransitionMap transitions;
	std::weak_ptr<State> lowlink;

	friend std::ostream& operator<<(std::ostream& os, const State& state)
	{
		os << std::to_string(state.index)
			<< " (" << std::to_string(state.indegree) << ")"
			<< "\t->\t{ ";
		for (const auto& transition : state.transitions)
		{
			os << std::to_string(transition.first.lock()->index) << " ";
		}
		os << "}";
		return os;
	}

	bool operator()(std::shared_ptr<State> lhs, std::shared_ptr<State> rhs) const
	{
		auto& lValues = lhs->values;
		auto& rValues = rhs->values;
		auto size = lValues.size() < rValues.size() ? lValues.size() : rValues.size();

		for (std::size_t i = 0; i < size; ++i)
		{
			auto ordering = lValues[i]->Cmp(*rValues[i]);
			if (ordering != 0)
				return ordering < 0;
		}

		return lValues.size() < rValues.size();
	}
};

class FiniteStateMachine
{
public:
	explicit FiniteStateMachine(const std::string& filePath, bool combineStates = true, bool onlyOutput = false);
	~FiniteStateMachine();

protected:
	void AddState(
		std::weak_ptr<State>& prevState,
		const uint64_t& timestamp,
		bool isInput,
		const std::vector<Change>& changes,
		bool combineStatesWithDuplicateValues
	);

public:
	uint64_t GetStateCount();

	uint64_t CombineSequences();

#ifndef COMBINED_STATES
	void RemoveInputStates();
#endif

	uint64_t CombineSCC();

	uint64_t MergeCircuits();

	void RenumberStates();

	void RelativeTimes();

	void CutToPart(const uint64_t& startIndex, const uint64_t& endIndex, bool ignoreBackEdges = false, uint64_t* tabooState = nullptr);

	std::string GetStateValues(const uint64_t& stateIndex) const;

	std::string GetTransitionTimes(const uint64_t& stateIndex) const;

	std::string PrintTimes() const;

	std::string PrintTimeAutomata(
		const uint64_t& startIndex = 0,
		const uint64_t& finalIndex = 0,
		const std::string& statePrefix = "s",
		unsigned short precision = 3) const;

	std::string PrintRightLinearGrammar(
		const uint64_t& startIndex = 0,
		const uint64_t& finalIndex = 0,
		const std::string& statePrefix = "s",
		const std::string& transitionPrefix = "",
		bool printProcentualDiff = false
	) const;

	std::string PrintRegularAutomota(
		const uint64_t& startIndex = 0,
		const uint64_t& finalIndex = 0,
		const std::string& statePrefix = "s",
		const std::string& transitionPrefix = "t",
		bool printProcentualDiff = false
	);

public:
	friend std::ostream& operator<<(std::ostream& os, const FiniteStateMachine& fsm)
	{
		for (auto& state : fsm.m_stateContainer)
			os << *state << std::endl;
		return os;
	}

protected:
	std::shared_ptr<State> m_startState;
	std::set<std::shared_ptr<State>, State> m_stateContainer;
};

using FSM = FiniteStateMachine;