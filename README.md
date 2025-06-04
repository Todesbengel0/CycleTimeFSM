# Finite State Machine Process Analyzer

## Overview

This project provides a tool for analyzing process durations in industrial control systems using automatically generated Finite State Machines (FSMs). It allows engineers and researchers to reverse-engineer control behavior from signal changes recorded in event logs—without prior knowledge of the Programmable Logic Controller (PLC) configuration.

The core functionality:
- Parses input/output signal changes from JSON-based event logs.
- Synthesizes and optimizes deterministic FSMs representing process flows.
- Supports state minimization, circuit merging, and cycle time analysis.
- Outputs DFA representations, right-linear grammars, and process timing information.

## Features

- FSM synthesis from real PLC signal logs
- State minimization and sequence combination
- Strongly connected component (SCC) detection
- Support for removing input states and measuring relative times
- Export to:
  - DFA format
  - Regular grammars
  - Time-annotated automata
- Duplicate state tracking (optional)

## Requirements

- **C++17 or higher**
- [nlohmann/json](https://github.com/nlohmann/json) header-only library (used for JSON parsing)

## Usage

### 1. Prepare Input

Create a JSON file (e.g., `TAreal.json`) with this structure:

```json
{
  "frames": [
    {
      "timestamp": 123456789,
      "input/output": true,
      "data": [
        {
          "participant": 0,
          "byte": [1, 0]
        }
      ]
    },
    ...
  ]
}
```

### 2. Compile

Use a C++17-capable compiler:

```bash
g++ -std=c++17 -o fsm main.cpp FiniteStateMachine.cpp StateValuesRegistry.cpp
```

### 3. Run

```bash
./fsm
```

The program will:
- Load the JSON file
- Build the FSM
- Optimize and renumber states
- Output a DFA description to `DFA.txt`

### 4. Output Files

- `DFA.txt`: Regular automaton in human-readable format
- Additional formats (grammar, timing) can be enabled in `main.cpp` via preprocessor flags

## Configuration

Enable or disable functionality by editing preprocessor macros in `main.cpp`:

```cpp
#define UniqueStates        // Enables state combination
//#define COUNT_DUPLICATES  // Tracks duplicate states
```

Optional functions in `FiniteStateMachine` include:
- `CombineSequences()`
- `CombineSCC()`
- `MergeCircuits()`
- `RelativeTimes()`
- `PrintTimes()`
- `PrintRegularAutomota()`
- `PrintRightLinearGrammar()`

## License

This project is part of an academic thesis and is shared under a permissive license (MIT or similar) for educational and research purposes. Attribution required.

## Contact

For inquiries, improvements, or citations, please contact:
**Dan Eisenkrämer**  
dan@eisenkraemer.com
