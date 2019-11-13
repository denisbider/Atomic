#pragma once

#include "AtPtrPair.h"
#include "AtSeq.h"


namespace At
{
	class HtmlBuilder;

	namespace Diff
	{

		struct DiffParams
		{
			// Maximum width of the sliding matrix used to calculate the best diff path. The units are matrix axis units.
			// A smaller width causes the sliding matrix to use less space, but will more often find a suboptimal diff.
			// A larger width increases the likelihood that the best diff will be found, but uses linearly more space.
			//
			// Each matrix axis unit represents an input unit that is present in both the old and new input streams.
			// Input units that are present only in one of the input streams are treated as a trivial attachment to the next axis unit.
			// The amount of space required for the sliding matrix is:
			//
			//   sizeof(Cell) * max(nrOldAxisUnits, nrNewAxisUnits) * maxMatrixWidth
			//
			// For example, if each cell is 32-bit, maxMatrixWidth = 800, and two almost identical files of 100,000 lines are diffed,
			// the matrix memory requirement will be 4 * 100000 * 800 = 320 MB.
			uint m_maxMatrixWidth { 800 };

			// When calculating the best diff path matrix, the number of points to assign to each move for maintaining momentum.
			// The number of points configured here is given to a diff path for each move that continues what the previous one did.
			// For example, this favors diffs with consolidated additions/removals (+++---) over diffs that alternate (+-+-+-).
			// Don't use large numbers or it can force use of a 64-bit matrix instead of 32-bit, increasing memory cost and reducing speed.
			uint m_quality_momentum { 1 };

			// When calculating the best diff path matrix, the number of points to assign for each unchanged unit in the diff output.
			// This favors diffs that actually find the longest common subsequence of the inputs, or a subsequence close to it.
			// Don't use large numbers or it can force use of a 64-bit matrix instead of 32-bit, increasing memory cost and reducing speed.
			uint m_quality_match { 5 };

			// Whether to include unchanged units in diff output.
			bool m_emitUnchanged {};

			// Set to write debug HTML output containing the full diff matrix
			HtmlBuilder* m_debugHtml {};
		};

		struct InputUnit
		{
			sizet m_seqNr;
			Seq   m_value;

			InputUnit(sizet seqNr, Seq value) : m_seqNr(seqNr), m_value(value) {}
		};

		enum class DiffDisposition { Unchanged, Added, Removed };
		enum class DiffInputSource { Old, New };

		struct DiffUnit
		{
			DiffDisposition m_disposition;
			DiffInputSource m_inputSource;
			InputUnit       m_inputUnit;

			DiffUnit(DiffDisposition disposition, DiffInputSource inputSource, InputUnit const& inputUnit)
				: m_disposition(disposition), m_inputSource(inputSource), m_inputUnit(inputUnit) {}
		};

		void DebugCss(HtmlBuilder& html);

		// Implements a diff algorithm with the following properties:
		// - High quality output: if computationally feasible, finds the longest common subsequence; prefers to group additions and removals together.
		// - Good worst-case performance: for large inputs, space/time cost is O(max(N,M)), but might not find best diff for large and complex inputs.
		// - Does not use recursion: uses a sliding matrix that is limited in width for easy control of the quality/performance tradeoff.
		// - Trivially handles common head and tail: the matrix algorithm is used between first and last difference in the inputs.
		void Generate(PtrPair<InputUnit> inputOld, PtrPair<InputUnit> inputNew, Vec<DiffUnit>& diff, DiffParams const& params);

	}
}
