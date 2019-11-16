#include "AtIncludes.h"
#include "AtDiff.h"

#include "AtHtmlBuilder.h"
#include "AtMap.h"


namespace At
{
	namespace Diff
	{
		namespace Internal
		{

			struct UnitPos
			{
				sizet          m_pos {};
				UnitPos const* m_next {};

				UnitPos(sizet pos, UnitPos const* next) : m_pos(pos), m_next(next) {}
			};


			struct UniqueUnit
			{
				Seq      m_value;
				sizet    m_nrInOld {};
				sizet    m_nrInNew {};
				UnitPos* m_posInOld {};			// Single-linked list, reverse order of occurrence, last position first
				UnitPos* m_posInNew {};			// Single-linked list, reverse order of occurrence, last position first
				bool     m_trivial {};

				UniqueUnit(Seq value) : m_value(value) {}

				bool IsAxisUnit            () const { return (m_nrInOld != 0) && (m_nrInNew != 0); }
				bool IsKeystoneSingular    () const { return (m_nrInOld == 1) && (m_nrInNew == 1); }
				bool IsKeystoneMultipleOld () const { return ((m_nrInOld == 1) && (m_nrInNew > 1)); }
				bool IsKeystoneMultipleNew () const { return ((m_nrInNew == 1) && (m_nrInOld > 1)); }
				bool IsKeystoneMultiple    () const { return IsKeystoneMultipleOld() || IsKeystoneMultipleNew(); }

				void AddOldPos(Vec<UnitPos>& storage, sizet pos) { ++m_nrInOld; AddPos(storage, pos, m_posInOld); }
				void AddNewPos(Vec<UnitPos>& storage, sizet pos) { ++m_nrInNew; AddPos(storage, pos, m_posInNew); }

			private:
				void AddPos(Vec<UnitPos>& storage, sizet pos, UnitPos*& posPtr) { posPtr = &storage.Add(pos, posPtr); }
			};


			struct Occurrence
			{
				InputUnit const* m_iu;
				UniqueUnit*      m_uu;

				Occurrence(InputUnit const* iu, UniqueUnit* uu) : m_iu(iu), m_uu(uu) {}
			};


			struct UniqueUnitMapEntry
			{
				Seq         m_value;
				UniqueUnit* m_uniqueUnit;

				UniqueUnitMapEntry(Seq value) : m_value(value) {}
				Seq Key() const { return m_value; }
			};

			using UniqueUnitMap = Map<UniqueUnitMapEntry>;


			struct UniqueUnitView
			{
				UniqueUnitMap   m_uum;
				Vec<Occurrence> m_occurrencesOld;
				Vec<Occurrence> m_occurrencesNew;

				void Build(PtrPair<InputUnit> inputOld, PtrPair<InputUnit> inputNew, DiffParams const& params)
				{
					m_uniqueUnitStorage.FixReserve(inputOld.Len() + inputNew.Len());
					m_unitPosStorage.FixReserve(inputOld.Len() + inputNew.Len());
					m_occurrencesOld.FixReserve(inputOld.Len());
					m_occurrencesNew.FixReserve(inputNew.Len());

					for (sizet i=0; i!=inputOld.Len(); ++i)
					{
						InputUnit const& iu = inputOld[i];
						UniqueUnit* uu = FindOrAddUniqueUnit(iu, params);
						uu->AddOldPos(m_unitPosStorage, i);
						m_occurrencesOld.Add(&iu, uu);
					}

					for (sizet i=0; i!=inputNew.Len(); ++i)
					{
						InputUnit const& iu = inputNew[i];
						UniqueUnit* uu = FindOrAddUniqueUnit(iu, params);
						uu->AddNewPos(m_unitPosStorage, i);
						m_occurrencesNew.Add(&iu, uu);
					}
				}

				uint64 MaxPossibleCellQuality(DiffParams const& params) const
				{
					uint64 maxAxisCellsOld = m_occurrencesOld.Len();
					uint64 maxAxisCellsNew = m_occurrencesNew.Len();
					uint64 maxMatchCount = PickMin(maxAxisCellsOld, maxAxisCellsNew);
					uint64 maxMomentumCount = m_occurrencesOld.Len() + m_occurrencesNew.Len();
					uint64 maxMatchQuality = PickMax(params.m_quality_match, params.m_quality_matchTrivial, params.m_quality_keystoneMultiple, params.m_quality_keystoneSingular);
					uint64 maxQuality = 1 + (maxMatchCount * maxMatchQuality) + (maxMomentumCount * params.m_quality_momentum);
					return maxQuality;
				}

			private:
				Vec<UniqueUnit> m_uniqueUnitStorage;
				Vec<UnitPos>    m_unitPosStorage;

				UniqueUnit* FindOrAddUniqueUnit(InputUnit const& iu, DiffParams const& params)
				{
					bool added {};
					UniqueUnitMapEntry entry { iu.m_value };
					UniqueUnitMap::It it = m_uum.FindOrAdd(added, std::move(entry));
					if (added)
					{
						it->m_uniqueUnit = &m_uniqueUnitStorage.Add(iu.m_value);
						if (params.m_isInputTrivial)
							it->m_uniqueUnit->m_trivial = params.m_isInputTrivial(iu.m_value);
					}
					return it->m_uniqueUnit;
				}
			};


			struct AxisUnit
			{
				// An AxisUnit for the SlidingMatrix consists of:
				// - zero or more Vec<Occurrence> positions corresponding to units that appear only on this axis, followed by
				// - the Vec<Occurrence> position of exactly one unit which appears on both axes.
				//
				// The units that appear only on this axis are mostly irrelevant to finding the longest common subsequence,
				// so they can be trivially bundled with the next unit on the axis which is relevant to finding the LCS.

				sizet             m_leadPos {};
				sizet             m_unitPos {};
				UniqueUnit const* m_uu      {};

				AxisUnit(sizet leadPos, sizet unitPos, UniqueUnit const* uu)
					: m_leadPos(leadPos), m_unitPos(unitPos), m_uu(uu) {}
			};


			struct MatrixAxis
			{
				Vec<AxisUnit>          m_axisUnits;
				Vec<Occurrence> const* m_occurrences;

				sizet Len() const { return m_axisUnits.Len(); }

				void Build(Vec<Occurrence> const& occurrences)
				{
					m_occurrences = &occurrences;

					sizet nrAxisUnits = CalculateNrAxisUnits(occurrences);
					m_axisUnits.FixReserve(nrAxisUnits);

					sizet leadPos {}, unitPos {};
					for (Occurrence const& o : occurrences)
					{
						if (!o.m_uu->IsAxisUnit())
							++unitPos;
						else
						{
							m_axisUnits.Add(leadPos, unitPos, o.m_uu);
							leadPos = ++unitPos;
						}
					}
				}

				void Debug_DumpAxis(HtmlBuilder& html) const
				{
					html.Table().Class("AtDiff");
					for (sizet axisIdx=0; axisIdx!=m_axisUnits.Len(); ++axisIdx)
					{
						AxisUnit const& u = m_axisUnits[axisIdx];
						html.Tr()
								.Td().UInt(axisIdx).EndTd()
								.Td();

						bool needBr {};
						for (sizet pos=u.m_leadPos; pos<=u.m_unitPos; ++pos)
						{
							if (needBr) html.Br(); else needBr = true;
							InputUnit const& iu = *(*m_occurrences)[pos].m_iu;
							html.UInt(iu.m_seqNr).T(": ").T(iu.m_value, Html::CharRefs::Escape);
						}

						html	.EndTd()
							.EndTr();
					}
					html.EndTable();
				}

			private:
				static sizet CalculateNrAxisUnits(Vec<Occurrence> const& occurrences)
				{
					sizet count {};
					for (Occurrence const& o : occurrences)
						if (o.m_uu->IsAxisUnit())
							++count;
					return count;
				}
			};


			struct MatrixAxes
			{
				MatrixAxis m_axisOld;
				MatrixAxis m_axisNew;

				void Build(UniqueUnitView const& uuv)
				{
					m_axisOld.Build(uuv.m_occurrencesOld);
					m_axisNew.Build(uuv.m_occurrencesNew);
				}
			};


			struct CellDir { enum { End, Diag, Right, Down }; };

			struct Cell
			{
				uint32 m_dir     :  2;
				uint32 m_quality : 30;
			};

			enum : uint32 { Cell_MaxQuality = (1U << 30) - 1U };

			static_assert(sizeof(Cell) == sizeof(uint32), "Unexpected Cell size");


			struct SlidingMatrix
			{
				// For small enough input streams, we calculate a matrix of size N*M representing additions/removals, and then
				// relatively trivially calculate the desirability of being at each position in the matrix. This allows for finding
				// the definite LCS or the most desirable common sequence; not necessarily longest, but e.g. containing most long runs.
				//
				// For larger input streams, calculating the full N*M matrix is infeasible, so we instead calculate a partial matrix
				// of a fixed width running along a central diagonal. Then if the most desirable common sequence is within the partial
				// matrix, great, we can easily find it. Otherwise, we find a less desirable solution that's within the partial matrix.

				~SlidingMatrix()
				{
					delete[] m_matrix;
				}

				void BuildAndSolve(MatrixAxes const& axes, Vec<DiffUnit>& diff, DiffParams const& params)
				{
					Build(axes, params);
					CalculateEdges(params);
					CalculateInner(params);

					if (params.m_debugHtml)
						Debug_Matrix(*params.m_debugHtml);

					Travel(axes, diff, params);
				}
			
			private:
				MatrixAxis const* m_rowAxis {};
				MatrixAxis const* m_colAxis {};

				bool IsEdgeRow(sizet row) { return row == m_rowAxis->Len(); }
				bool IsEdgeCol(sizet col) { return col == m_colAxis->Len(); }
				bool IsEdgeCell(sizet row, sizet col) { return IsEdgeRow(row) || IsEdgeCol(col); }

				UniqueUnit const* GetRowUnit(sizet row) { return m_rowAxis->m_axisUnits[row].m_uu; }
				UniqueUnit const* GetColUnit(sizet col) { return m_colAxis->m_axisUnits[col].m_uu; }
				bool IsRowColUnitMatch(sizet row, sizet col, UniqueUnit const*& uu) { UniqueUnit const* ru = GetRowUnit(row); uu = GetColUnit(col); return ru == uu; }
				sizet NrLeadUnitsAtRow(sizet row) { AxisUnit const& x = m_rowAxis->m_axisUnits[row]; return x.m_unitPos - x.m_leadPos; }
				sizet NrLeadUnitsAtCol(sizet col) { AxisUnit const& x = m_colAxis->m_axisUnits[col]; return x.m_unitPos - x.m_leadPos; }

				Vec<sizet> m_rowOffsets;
				Cell*      m_matrix  {};	// Each cell receives the calculated quality for its coordinate in the matrix
				bool       m_colsNew {};
				sizet      m_nrCols  {};	// Number of columns in each row, not including initial offset cell. This includes unallocated columns and right edge cell
				sizet      m_width   {};	// Number of actual (allocated) columns in each row. This may or may not include the right edge cell
				sizet      m_nrRows  {};	// Number of rows. This includes the bottom edge row

				Cell* GetRow(sizet row) { return m_matrix + (row * m_width); }

				Cell* GetCell(sizet row, sizet col)
				{
					if (row >= m_nrRows) return nullptr;
					sizet offset = m_rowOffsets[row];
					if (col < offset) return nullptr;
					if (col >= offset + m_width) return nullptr;
					return GetRow(row) + (col - offset);
				}

				void Build(MatrixAxes const& axes, DiffParams const& params)
				{
					// Set up the matrix so that the longer input is the row axis
					if (axes.m_axisNew.Len() < axes.m_axisOld.Len())
					     { m_colsNew = true;  m_colAxis = &axes.m_axisNew; m_rowAxis = &axes.m_axisOld; }
					else { m_colsNew = false; m_colAxis = &axes.m_axisOld; m_rowAxis = &axes.m_axisNew; }

					m_nrRows = m_rowAxis->Len() + 1;	// Additional row for bottom edge
					m_nrCols = m_colAxis->Len() + 1;	// Additional column for right edge
					m_width = m_nrCols;

					enum : uint { MinMatrixWidth = 2U };
					uint const slidingMatrixWidth = PickMax<uint>(MinMatrixWidth, params.m_slidingMatrixWidth);
					if (m_width > slidingMatrixWidth)
					{
						uint64 nrCells = ((uint64) m_nrRows) * ((uint64) m_nrCols);
						if (nrCells > params.m_maxFullMatrixCells)
						{
							m_width = params.m_maxFullMatrixCells / m_nrRows;
							if (m_width < slidingMatrixWidth)
								m_width = slidingMatrixWidth;
						}
					}

					m_matrix = new Cell[m_nrRows * m_width];
					m_rowOffsets.FixReserve(m_nrRows);

					sizet slack = ((m_nrRows - m_nrCols) + m_width) / 2;
					for (sizet row=0, offset=0; row!=m_nrRows; ++row)
					{
						m_rowOffsets.Add(offset);
						if (row >= slack && (offset + m_width < m_nrCols))
							++offset;
					}
				}

				// "row" and "col" must point to an inner cell or edge cell where the move would begin. They must not point to the bottom-right corner cell.
				// "next" may point to an inner cell or an edge cell. If "next" is null, the function returns 0.
				uint64 MoveQualityAndMomentum(sizet row, sizet col, uint dir, Cell const* next, DiffParams const& params)
				{
					if (!next)
						return 0;

					uint const momentum = params.m_quality_momentum;
					uint64 v {};

					// Intrinsic momentum from advancing over the current cell
					if (dir == CellDir::Right)
					{
						// A transition toward the right may have intrinsic momentum from the lead units and the current unit to be consumed
						v += NrLeadUnitsAtCol(col) * momentum;
					}
					else if (dir == CellDir::Down)
					{
						// A transition downward may have intrinsic momentum from the lead units and the current unit to be consumed
						v += NrLeadUnitsAtRow(row) * momentum;
					}
					else if (dir == CellDir::Diag)
					{
						// A diagonal transition may have intrinsic momentum from lead units to be consumed.
						// However, such lead units do not synergize with each other or with the current unit
						v += SatSub<sizet>(NrLeadUnitsAtCol(col), 1U) * momentum;
						v += SatSub<sizet>(NrLeadUnitsAtRow(row), 1U) * momentum;
					}

					v += next->m_quality;

					// Momentum from synergy with the next cell
					if (dir == CellDir::Diag)
					{
						// A diagonal transition has synergy momentum with next cell only if next cell is also diagonal and does not have lead units
						if (next->m_dir == CellDir::Diag && !NrLeadUnitsAtCol(col+1) && !NrLeadUnitsAtRow(row+1))
							v += momentum;
					}
					else
					{
						// A transition toward the right or downward has synergy momentum with next cell if next cell maintains same direction,
						// OR if the next cell is diagonal and has the synergistic type of lead unit
						if (next->m_dir == dir)
							v += momentum;
						else if (next->m_dir == CellDir::Diag)
						{
							if (dir == CellDir::Right && (0 != NrLeadUnitsAtCol(col+1)))
								v += momentum;
							else if (dir == CellDir::Down && (0 != NrLeadUnitsAtRow(row+1)))
								v += momentum;
						}
					}

					return v;
				}

				void CalculateEdges(DiffParams const& params)
				{
					// The bottom and right edges of the matrix are the states we arrive in after fully advancing across the corresponding axis.
					// At that point, it only remains possible to go in one remaining direction, or we are already at end.

					uint const momentum = params.m_quality_momentum;

					// Bottom-right corner
					sizet row = m_nrRows-1, col = m_nrCols-1;
					Cell* corner = GetCell(row, col);
					EnsureThrow(nullptr != corner);
					Cell* cell = corner;
					cell->m_dir = CellDir::End;
					cell->m_quality = 1;

					// Bottom edge
					sizet offset = m_rowOffsets[row];
					while (col != offset)
					{
						--col;
						--cell;

						cell->m_dir = CellDir::Right;
						cell->m_quality = MoveQualityAndMomentum(row, col, CellDir::Right, cell+1, params);
					}

					// Right edge
					col = m_nrCols-1;
					cell = corner;

					while (row != 0)
					{
						--row;
						offset = m_rowOffsets[row];
						if (offset + m_width < m_nrCols)
							break;

						Cell* next = cell;
						cell = GetCell(row, col);
						EnsureThrow(nullptr != cell);
						cell->m_dir = CellDir::Down;
						cell->m_quality = MoveQualityAndMomentum(row, col, CellDir::Down, next, params);
					}
				}

				void CalculateInner(DiffParams const& params)
				{
					if (m_nrRows > 1 && m_nrCols > 1)
					{
						// We calculate the matrix starting from the finish (bottom right) and working back to the start (top left).
						// Each coordinate is assigned a quality based on the best move available from there (right, down or diagonal)
						// and the quality of the next cell after performing that move.

						sizet colFrom = m_nrCols - 2;
						for (sizet row=m_nrRows-1; row!=0; )
						{
							--row;

							Cell* cell;
							while (true)
							{
								cell = GetCell(row, colFrom);
								if (cell) break;
								EnsureThrow(colFrom != 0);
								--colFrom;
							}

							sizet offset = m_rowOffsets[row];
							EnsureThrow(colFrom >= offset);

							for (sizet col=colFrom+1; col!=offset; --cell)
							{
								--col;

								Cell const* diag {};
								uint64      diagQuality {};

								UniqueUnit const* uu;
								if (IsRowColUnitMatch(row, col, uu))
								{
									uint matchQuality;
									     if (uu->m_trivial)            matchQuality = params.m_quality_matchTrivial;
									else if (uu->IsKeystoneSingular()) matchQuality = params.m_quality_keystoneSingular;
									else if (uu->IsKeystoneMultiple()) matchQuality = params.m_quality_keystoneMultiple;
									else                               matchQuality = params.m_quality_match;

									diag = GetCell(row+1, col+1);
									diagQuality = matchQuality + MoveQualityAndMomentum(row, col, CellDir::Diag, diag, params);
								}

								Cell const* right = GetCell(row,   col+1 );
								Cell const* down  = GetCell(row+1, col   );

								uint64 rightQuality = MoveQualityAndMomentum(row, col, CellDir::Right, right, params);
								uint64 downQuality  = MoveQualityAndMomentum(row, col, CellDir::Down,  down,  params);

								     if (diagQuality  > PickMax(rightQuality, downQuality)) { cell->m_dir = CellDir::Diag;  cell->m_quality = diagQuality;  }
								else if (rightQuality > PickMax(diagQuality,  downQuality)) { cell->m_dir = CellDir::Right; cell->m_quality = rightQuality; }
								else                                                        { cell->m_dir = CellDir::Down;  cell->m_quality = downQuality;  }
							}
						}
					}
				}

				void Debug_Matrix(HtmlBuilder& html)
				{
					enum { Debug_MaxAxisUnits = 100 };
					if (m_colAxis->Len() <= Debug_MaxAxisUnits && m_rowAxis->Len() <= Debug_MaxAxisUnits)
						Debug_DumpMatrix(html);
					else
					{
						html.Table().Class("diffSummary")
								.Tr().Th().T("Vertical axis ").T(m_colsNew ? "(old)" : "(new)").EndTh()		.Td().UInt(m_rowAxis->Len()).EndTd()	.EndTr()
								.Tr().Th().T("Horizontal axis ").T(m_colsNew ? "(new)" : "(old)").EndTh()	.Td().UInt(m_colAxis->Len()).EndTd()	.EndTr()
								.Tr().Th().T("Sliding matrix width").EndTh()								.Td().UInt(m_width).EndTd()				.EndTr()
							.EndTable();
					}
				}

				void Debug_DumpMatrix(HtmlBuilder& html)
				{
					html.H3().T("Vertical axis ").T(m_colsNew ? "(old)" : "(new)").EndH3();
					m_rowAxis->Debug_DumpAxis(html);

					html.H3().T("Horizontal axis ").T(m_colsNew ? "(new)" : "(old)").EndH3();
					m_colAxis->Debug_DumpAxis(html);

					html.H3().T("Matrix").EndH3()
						.Table().Class("diffMatrix")
							.Tr()
								.Th().EndTh();

					for (sizet col=0; col!=m_nrCols; ++col)
						html	.Th().UInt(col).EndTh();
								
					html	.EndTr();

					sizet pathRow {}, pathCol {};
					for (sizet row=0; row!=m_nrRows; ++row)
					{
						html.Tr()
								.Th().UInt(row).EndTh();

						for (sizet col=0; col!=m_nrCols; ++col)
						{
							bool path = (col == pathCol && row == pathRow);
							UniqueUnit const* uu;
							bool match = !IsEdgeCell(row, col) && IsRowColUnitMatch(row, col, uu);
							html.Td();
							if (path && match) html.Class("path match");
							else if (path) html.Class("path");
							else if (match) html.Class("match");
							Cell* cell = GetCell(row, col);
							if (cell)
							{
								html.UInt(cell->m_quality).Br();

								switch (cell->m_dir)
								{
								case CellDir::Right: html.T("&rarr;"); if (path) ++pathCol; break;
								case CellDir::Down:  html.T("&darr;"); if (path) ++pathRow; break;
								case CellDir::Diag:  html.T("&searr;"); if (path) { ++pathCol; ++pathRow; } break;
								case CellDir::End:   html.T("&nbsp;"); break;
								default:             EnsureThrow(!"Unrecognized cell direction");
								}
							}
							html.EndTd();
						}
						html.EndTr();
					}
					html.EndTable();
				}

				void Travel(MatrixAxes const& axes, Vec<DiffUnit>& diff, DiffParams const& params)
				{
					AxisUnit const* pAxisOld = axes.m_axisOld.m_axisUnits.begin();
					AxisUnit const* pAxisNew = axes.m_axisNew.m_axisUnits.begin();
					Occurrence const* pOccurOld = axes.m_axisOld.m_occurrences->begin();
					Occurrence const* pOccurNew = axes.m_axisNew.m_occurrences->begin();
					bool lastRemoved {};

					auto advanceOld = [&]
						{
							for (sizet i=pAxisOld->m_leadPos; i<=pAxisOld->m_unitPos; ++i)
							{
								diff.Add(DiffDisposition::Removed, DiffInputSource::Old, *(pOccurOld->m_iu));
								++pOccurOld;
							}
							++pAxisOld;
							lastRemoved = true;
						};

					auto advanceNew = [&]
						{
							for (sizet i=pAxisNew->m_leadPos; i<=pAxisNew->m_unitPos; ++i)
							{
								diff.Add(DiffDisposition::Added, DiffInputSource::New, *(pOccurNew->m_iu));
								++pOccurNew;
							}
							++pAxisNew;
							lastRemoved = false;
						};

					auto advanceRight = [&] { if (m_colsNew) advanceNew(); else advanceOld(); };
					auto advanceDown  = [&] { if (m_colsNew) advanceOld(); else advanceNew(); };

					auto advanceDiag = [&]
						{
							if (lastRemoved)
							{
								for (sizet i=pAxisOld->m_leadPos; i!=pAxisOld->m_unitPos; ++i) { diff.Add(DiffDisposition::Removed, DiffInputSource::Old, *(pOccurOld->m_iu)); ++pOccurOld; }
								for (sizet i=pAxisNew->m_leadPos; i!=pAxisNew->m_unitPos; ++i) { diff.Add(DiffDisposition::Added,   DiffInputSource::New, *(pOccurNew->m_iu)); ++pOccurNew; }
							}
							else
							{
								for (sizet i=pAxisNew->m_leadPos; i!=pAxisNew->m_unitPos; ++i) { diff.Add(DiffDisposition::Added,   DiffInputSource::New, *(pOccurNew->m_iu)); ++pOccurNew; }
								for (sizet i=pAxisOld->m_leadPos; i!=pAxisOld->m_unitPos; ++i) { diff.Add(DiffDisposition::Removed, DiffInputSource::Old, *(pOccurOld->m_iu)); ++pOccurOld; }
							}

							if (params.m_emitUnchanged)
								diff.Add(DiffDisposition::Unchanged, DiffInputSource::New, *(pOccurNew->m_iu));

							++pOccurOld; ++pOccurNew;
							++pAxisOld; ++pAxisNew;
							lastRemoved = false;
						};

					sizet row {}, col {};
					Cell const* cell = GetCell(row, col);

					while (CellDir::End != cell->m_dir)
					{
						     if (CellDir::Right == cell->m_dir) { advanceRight(); ++col; EnsureThrow(col != m_nrCols); ++cell; }
						else if (CellDir::Down  == cell->m_dir) { advanceDown();  ++row;        cell = GetCell(row, col); EnsureThrow(cell); }
						else if (CellDir::Diag  == cell->m_dir) { advanceDiag();  ++row; ++col; cell = GetCell(row, col); EnsureThrow(cell); }
						else EnsureThrow(!"Unrecognized cell direction");
					}

					EnsureAbort(pOccurOld <= axes.m_axisOld.m_occurrences->end());
					EnsureAbort(pOccurNew <= axes.m_axisNew.m_occurrences->end());
					EnsureAbort(pAxisOld == axes.m_axisOld.m_axisUnits.end());
					EnsureAbort(pAxisNew == axes.m_axisNew.m_axisUnits.end());

					if (lastRemoved)
					{
						while (pOccurOld != axes.m_axisOld.m_occurrences->end()) { diff.Add(DiffDisposition::Removed, DiffInputSource::Old, *(pOccurOld->m_iu)); ++pOccurOld; }
						while (pOccurNew != axes.m_axisNew.m_occurrences->end()) { diff.Add(DiffDisposition::Added,   DiffInputSource::New, *(pOccurNew->m_iu)); ++pOccurNew; }
					}
					else
					{
						while (pOccurNew != axes.m_axisNew.m_occurrences->end()) { diff.Add(DiffDisposition::Added,   DiffInputSource::New, *(pOccurNew->m_iu)); ++pOccurNew; }
						while (pOccurOld != axes.m_axisOld.m_occurrences->end()) { diff.Add(DiffDisposition::Removed, DiffInputSource::Old, *(pOccurOld->m_iu)); ++pOccurOld; }
					}
				}
			};


			void DiffCommonTail(PtrPair<InputUnit> commonTailNew, Vec<DiffUnit>& diff, DiffParams const& params)
			{
				if (params.m_emitUnchanged)
				{
					while (commonTailNew.Any())
					{
						diff.Add(DiffDisposition::Unchanged, DiffInputSource::New, commonTailNew.First());
						commonTailNew.PopFirst();
					}
				}
			}

			
			struct Parcel
			{
				PtrPair<InputUnit>  m_inputOld;
				PtrPair<InputUnit>  m_inputNew;
				PtrPair<InputUnit>  m_commonTailNew;

				Opt<UniqueUnitView> m_uuv;
				uint64              m_estMaxNrCells {};
				bool                m_mustSplit {};		// If input size and parameters could cause cell quality integer overflow, must split the parcel by force if needed

				Parcel() {}
				Parcel(PtrPair<InputUnit> inputOld, PtrPair<InputUnit> inputNew) : m_inputOld(inputOld), m_inputNew(inputNew) {}

				void Build(DiffParams const& params)
				{
					m_uuv.Init();
					m_uuv->Build(m_inputOld, m_inputNew, params);
					m_mustSplit = (m_uuv->MaxPossibleCellQuality(params) > Cell_MaxQuality);

					uint64 const maxAxisCellsOld = m_inputOld.Len() + 1;
					uint64 const maxAxisCellsNew = m_inputNew.Len() + 1;
					m_estMaxNrCells = maxAxisCellsOld * maxAxisCellsNew;

					if (!m_mustSplit)
						m_mustSplit = m_estMaxNrCells > params.m_forceSplitNrCells;
				}

				bool ShouldSplit(DiffParams const& params) const
				{
					if (m_mustSplit) return true;
					if (params.m_preferNoSplit) return false;
					return m_estMaxNrCells > params.m_maxFullMatrixCells;
				}

				// Resets m_uuv if successful
				bool TrySplit(Parcel& other)
				{
					CommonRun cr;
					if (FindBestApparentCommonRun(cr))
					{
						other.m_inputOld.Set(cr.m_commonRunOld.end(), m_inputOld.end());
						other.m_inputNew.Set(cr.m_commonRunNew.end(), m_inputNew.end());
						other.m_commonTailNew = m_commonTailNew;

						m_inputOld.Set(m_inputOld.begin(), cr.m_commonRunOld.begin());
						m_inputNew.Set(m_inputNew.begin(), cr.m_commonRunNew.begin());
						m_commonTailNew = cr.m_commonRunNew;

						m_uuv.Clear();
						return true;
					}

					if (m_mustSplit)
					{
						InputUnit const* halfWayOld = m_inputOld.begin() + (m_inputOld.Len() / 2);
						InputUnit const* halfWayNew = m_inputNew.begin() + (m_inputNew.Len() / 2);

						other.m_inputOld.Set(halfWayOld, m_inputOld.end());
						other.m_inputNew.Set(halfWayNew, m_inputNew.end());
						other.m_commonTailNew = m_commonTailNew;

						m_inputOld.Set(m_inputOld.begin(), halfWayOld);
						m_inputNew.Set(m_inputNew.begin(), halfWayNew);
						m_commonTailNew.Clear();

						m_uuv.Clear();
						return true;
					}

					return false;
				}

				void Process(Vec<DiffUnit>& diff, DiffParams const& params)
				{
					EnsureThrow(!m_mustSplit);

					if (params.m_debugHtml)
						Debug_Parcel(*params.m_debugHtml);

					MatrixAxes axes;
					axes.Build(m_uuv.Ref());

					SlidingMatrix matrix;
					matrix.BuildAndSolve(axes, diff, params);

					DiffCommonTail(m_commonTailNew, diff, params);
				}

			private:

				struct CommonRun
				{
					PtrPair<InputUnit> m_commonRunOld;
					PtrPair<InputUnit> m_commonRunNew;
				};

				bool FindBestApparentCommonRun(CommonRun& cr) const
				{
					if (m_uuv->m_occurrencesOld.Len() > m_uuv->m_occurrencesNew.Len())
						return FindBestApparentCommonRun(cr, &CommonRun::m_commonRunOld, &CommonRun::m_commonRunNew, m_uuv->m_occurrencesOld, m_uuv->m_occurrencesNew, &UniqueUnit::m_posInOld);
					else
						return FindBestApparentCommonRun(cr, &CommonRun::m_commonRunNew, &CommonRun::m_commonRunOld, m_uuv->m_occurrencesNew, m_uuv->m_occurrencesOld, &UniqueUnit::m_posInNew);
				}

				// Parameterized so that we can run the algorithm on the shorter axis (columns), regardless of whether the shorter axis is old or new.
				// These are NOT the same rows and columns used in SlidingMatrix: once MatrixAxes is built, which axis is longer vs. shorter may be reversed.
				// We just use the convention here that rows = longer, cols = shorter.
				bool FindBestApparentCommonRun(CommonRun& cr, PtrPair<InputUnit> CommonRun::* commonRunRows, PtrPair<InputUnit> CommonRun::* commonRunCols,
					Vec<Occurrence> const& occurrenceRows, Vec<Occurrence> const& occurrenceCols, UnitPos* UniqueUnit::* posInRows) const
				{
					uint64 const ourMatrixArea = ((uint64) occurrenceRows.Len()) * ((uint64) occurrenceCols.Len());
					uint64 const targetSubmatrixArea = ourMatrixArea / 2;

					sizet bestRun_len {}, bestRun_submatrixArea = SIZE_MAX;
					sizet matchCol {};
					while (matchCol != occurrenceCols.Len())
					{
						Occurrence const* o = &occurrenceCols[matchCol];
						UniqueUnit const* uu = o->m_uu;
						if (!uu->IsKeystoneSingular())
							++matchCol;
						else
						{
							// We found a singular keystone match - a unit that appears exactly once in both input AND output
							sizet matchRow = (uu->*posInRows)->m_pos;

							// How far back does this run go?
							sizet startRow = matchRow, startCol = matchCol;
							while (startRow && startCol)
							{
								--startRow; --startCol;
								if (occurrenceRows[startRow].m_uu != occurrenceCols[startCol].m_uu)
									{ ++startRow; ++startCol; break; }
							}

							// How far forward does this run go?
							while (true)
							{
								++matchRow; ++matchCol;
								if (matchRow == occurrenceRows.Len()) break;
								if (matchCol == occurrenceCols.Len()) break;
								if (occurrenceRows[matchRow].m_uu != occurrenceCols[matchCol].m_uu) break;
							}

							sizet runLenRows = (matchRow - startRow);
							sizet runLenCols = (matchCol - startCol);
							EnsureThrow(runLenRows == runLenCols);

							// If we don't have to split, is the run significant enough to split on?
							sizet maxPossibleRunLen = PickMin(occurrenceRows.Len(), occurrenceCols.Len());
							if (m_mustSplit || (runLenRows >= maxPossibleRunLen / 10))
							{
								// Is the new run also placed well enough?
								sizet rowsAfter = occurrenceRows.Len() - matchRow;
								sizet colsAfter = occurrenceCols.Len() - matchCol;

								// If we split based on this run, it will create two smaller submatrices
								uint64 firstSubmatrixArea  = ((uint64) startRow  ) * ((uint64) startCol  );
								uint64 secondSubmatrixArea = ((uint64) rowsAfter ) * ((uint64) colsAfter );
								uint64 submatrixArea = firstSubmatrixArea + secondSubmatrixArea;

								// A split must save enough matrix calculations to be worth doing
								if (submatrixArea <= targetSubmatrixArea)
								{
									// If we have a previous adequate run, pick the longest. If they are equal length, pick the best positioned
									if ((bestRun_len  < runLenRows) ||
										(bestRun_len == runLenRows && bestRun_submatrixArea > submatrixArea))
									{
										bestRun_len = runLenRows;
										bestRun_submatrixArea = submatrixArea;

										(cr.*commonRunRows).Set(occurrenceRows[startRow].m_iu, occurrenceRows[matchRow-1].m_iu + 1);
										(cr.*commonRunCols).Set(occurrenceCols[startCol].m_iu, occurrenceCols[matchCol-1].m_iu + 1);
										EnsureThrow((cr.*commonRunRows).Len() == runLenRows);
										EnsureThrow((cr.*commonRunCols).Len() == runLenCols);
									}
								}
							}
						}
					}

					return 0 != bestRun_len;
				}

				void Debug_Parcel(HtmlBuilder& html)
				{
					sizet nrUniqueAxisUnits {}, nrKeystonesSingular {}, nrKeystonesMultiple {};
					for (UniqueUnitMapEntry const& ue : m_uuv->m_uum)
					{
						if (ue.m_uniqueUnit->IsAxisUnit())         ++nrUniqueAxisUnits;
						if (ue.m_uniqueUnit->IsKeystoneSingular()) ++nrKeystonesSingular;
						if (ue.m_uniqueUnit->IsKeystoneMultiple()) ++nrKeystonesMultiple;
					}

					html.H2("Parcel")
						.Table().Class("diffSummary")
							.Tr().Th().T("Old input length").EndTh()		.Td().UInt(m_inputOld.Len()).EndTd()		.EndTr()
							.Tr().Th().T("New input length").EndTh()		.Td().UInt(m_inputNew.Len()).EndTd()		.EndTr()
							.Tr().Th().T("Common tail length").EndTh()		.Td().UInt(m_commonTailNew.Len()).EndTd()	.EndTr()
							.Tr().Th().T("Nr unique units").EndTh()			.Td().UInt(m_uuv->m_uum.Len()).EndTd()		.EndTr()
							.Tr().Th().T("Nr unique axis units").EndTh()	.Td().UInt(nrUniqueAxisUnits).EndTd()		.EndTr()
							.Tr().Th().T("Nr keystones singular").EndTh()	.Td().UInt(nrKeystonesSingular).EndTd()		.EndTr()
							.Tr().Th().T("Nr keystones multiple").EndTh()	.Td().UInt(nrKeystonesMultiple).EndTd()		.EndTr()
						.EndTable();
				}
			};


			void TrivialDiff(PtrPair<InputUnit>& inputOld, PtrPair<InputUnit>& inputNew, Vec<DiffUnit>& diff, DiffParams const& params)
			{
				while (inputOld.Any() && inputNew.Any())
				{
					if (inputOld.First().m_value != inputNew.First().m_value)
						return;

					if (params.m_emitUnchanged)
						diff.Add(DiffDisposition::Unchanged, DiffInputSource::New, inputNew.First());

					inputOld.PopFirst();
					inputNew.PopFirst();
				}

				while (inputOld.Any())
				{
					diff.Add(DiffDisposition::Removed, DiffInputSource::Old, inputOld.First());
					inputOld.PopFirst();
				}

				while (inputNew.Any())
				{
					diff.Add(DiffDisposition::Added, DiffInputSource::New, inputNew.First());
					inputNew.PopFirst();
				}
			}


			void CutCommonTail(PtrPair<InputUnit>& inputOld, PtrPair<InputUnit>& inputNew, PtrPair<InputUnit>& commonTailNew)
			{
				InputUnit const* origEndNew = inputNew.end();

				while (inputOld.Any() && inputNew.Any())
				{
					if (inputOld.Last().m_value != inputNew.Last().m_value)
						break;

					inputOld.PopLast();
					inputNew.PopLast();
				}

				commonTailNew.Set(inputNew.end(), origEndNew);
			}

			void CutCommonTail(Parcel& parcel)
			{
				CutCommonTail(parcel.m_inputOld, parcel.m_inputNew, parcel.m_commonTailNew);
			}

		}	// Internal

		using namespace Internal;


		DiffParams::DiffParams()
		{
			m_isInputTrivial = [] (Seq x) -> bool { x.TrimRight(); return !x.n; };
		}


		void DebugCss(HtmlBuilder& html)
		{
			html.AddCss("body "                           "{ font-size: x-small } "
						"table "                          "{ margin-bottom: 10px; } "
						"table.diffSummary th "           "{ background: #eee; font-weight: normal; padding: 0px 5px 0px 5px; } "
						"table.diffSummary td "           "{ padding: 0px 5px 0px 5px; } "
						"table.diffMatrix "               "{ border-collapse: collapse } "
						"table.diffMatrix th "            "{ border: 1px solid #666; background: #eee; font-weight: normal; padding: 0px 5px 0px 5px; } "
						"table.diffMatrix td "            "{ border: 1px solid #666; text-align: center; padding: 0px 5px 0px 5px; } "
						"table.diffMatrix td.path "       "{ background: #def; } "
						"table.diffMatrix td.match "      "{ background: #dfe; } "
						"table.diffMatrix td.path.match " "{ background: #ddf7f7; } ");
		}


		void Generate(PtrPair<InputUnit> inputOld, PtrPair<InputUnit> inputNew, Vec<DiffUnit>& diff, DiffParams const& params)
		{
			diff.ReserveExact(inputOld.Len() + inputNew.Len());

			TrivialDiff(inputOld, inputNew, diff, params);
			EnsureThrow(inputOld.Any() == inputNew.Any());

			if (inputOld.Any())
			{
				Vec<Parcel> parcels;
				parcels.Add(inputOld, inputNew);
				CutCommonTail(parcels.Last());

				sizet parcelIdx {};
				while (parcelIdx != parcels.Len())
				{
					Parcel& parcel = parcels[parcelIdx];
					parcel.Build(params);

					if (parcel.ShouldSplit(params))
					{
						Parcel otherParcel;
						if (parcel.TrySplit(otherParcel))
						{
							parcels.Insert(parcelIdx+1, std::move(otherParcel));
							continue;
						}
					}

					parcel.Process(diff, params);
					++parcelIdx;
				}
			}
		}

	}
}
