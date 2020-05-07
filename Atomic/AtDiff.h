#pragma once

#include "AtSeq.h"
#include "AtSlice.h"


namespace At
{
	namespace Diff
	{

		struct InputUnit
		{
			sizet m_seqNr;
			Seq   m_value;

			InputUnit(sizet seqNr, Seq value) : m_seqNr(seqNr), m_value(value) {}
		};

		struct DiffParams
		{
			// Disable to find the minimal edit script, regardless of number of steps
			bool m_limitSteps { true };
		};

		enum class DiffDisposition { Unchanged, Added, Removed };

		struct DiffUnit
		{
			DiffDisposition m_disposition;
			InputUnit       m_inputUnit;

			DiffUnit(DiffDisposition disposition, InputUnit const& inputUnit)
				: m_disposition(disposition), m_inputUnit(inputUnit) {}
		};

		// Implements a diff algorithm with the following properties:
		// - High quality output: if computationally feasible, finds the longest common subsequence; prefers to group additions and removals together.
		// - Good worst-case performance: for large inputs, space/time cost is O(max(N,M)), but might not find best diff for large and complex inputs.
		// - Does not use recursion: uses a sliding matrix that is limited in width for easy control of the quality/performance tradeoff.
		// - Trivially handles common head and tail: the matrix algorithm is used between first and last difference in the inputs.
		void Generate(Slice<InputUnit> inputOld, Slice<InputUnit> inputNew, Vec<DiffUnit>& diff, DiffParams const& params);

	}
}
