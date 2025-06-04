#include "FiniteStateMachine.h"

#define UniqueStates

int main()
{
    FSM fsm("TAreal.json");

    //std::cout << "Total Number of States: " << fsm.GetStateCount() << std::endl;

#ifdef COUNT_DUPLICATES
    std::cout << "Duplicates: " << StateValuesRegistry::GetNumberOfDuplicates() << std::endl;
    std::cout << "Unique Input States: " << StateValuesRegistry::GetUniqueInputStates()
        << ", Unique Output States: " << StateValuesRegistry::GetUniqueOutputStates() << std::endl;
#endif
    
#ifdef UniqueStates
	std::cout << "Linear Combine combined " << std::to_string(fsm.CombineSequences()) << " states.";
	std::cout << " => New Total Number of States: " << fsm.GetStateCount() << std::endl;
	//std::cout << fsm.CombineSCC() << " states were part of any circuit." << std::endl;
    
	fsm.RenumberStates();
	//fsm.RemoveInputStates();
    //fsm.RelativeTimes();

	//uint64_t taboo = 75;
	//fsm.CutToPart(40, 71, true, &taboo);

    //std::cout << fsm.PrintTimes() << std::endl;

	std::ofstream dfaFile("DFA.txt");
	dfaFile << fsm.PrintRegularAutomota(0, 0, "q", "t");
	dfaFile.close();
    
	//std::ofstream regFile("RLG.txt");
	//regFile << fsm.PrintRightLinearGrammar(0, 0, "q", "y");
	//regFile.close();
#endif
}