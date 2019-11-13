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

			struct UniqueUnit
			{
				Seq  m_value;
				bool m_isInOld : 1;
				bool m_isInNew : 1;

				bool IsAxisUnit() const { return m_isInOld && m_isInNew; }
				UniqueUnit(Seq value) : m_value(value), m_isInOld(false), m_isInNew(false) {}
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
				Vec<Occurrence> m_occurrencesOld;
				Vec<Occurrence> m_occurrencesNew;

				void Build(PtrPair<InputUnit> inputOld, PtrPair<InputUnit> inputNew)
				{
					UniqueUnitMap uum;
					m_uniqueUnitStorage.FixReserve(inputOld.Len() + inputNew.Len());
					m_occurrencesOld.FixReserve(inputOld.Len());
					m_occurrencesNew.FixReserve(inputNew.Len());

					for (sizet i=0; i!=inputOld.Len(); ++i)
					{
						InputUnit const& iu = inputOld[i];
						UniqueUnit* uu = FindOrAddUniqueUnit(uum, iu);
						uu->m_isInOld = true;
						m_occurrencesOld.Add(&iu, uu);
					}

					for (sizet i=0; i!=inputNew.Len(); ++i)
					{
						InputUnit const& iu = inputNew[i];
						UniqueUnit* uu = FindOrAddUniqueUnit(uum, iu);
						uu->m_isInNew = true;
						m_occurrencesNew.Add(&iu, uu);
					}
				}

			private:
				Vec<UniqueUnit> m_uniqueUnitStorage;

				UniqueUnit* FindOrAddUniqueUnit(UniqueUnitMap& uum, InputUnit const& iu)
				{
					bool added {};
					UniqueUnitMapEntry entry { iu.m_value };
					UniqueUnitMap::It it = uum.FindOrAdd(added, std::move(entry));
					if (added)
						it->m_uniqueUnit = &m_uniqueUnitStorage.Add(iu.m_value);
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

				uint64 MaxPossibleCellQuality(DiffParams const& params) const
				{
					uint64 nrAxisOld = m_axisOld.m_axisUnits.Len();
					uint64 nrAxisNew = m_axisNew.m_axisUnits.Len();
					uint64 maxMatches = PickMin(nrAxisOld, nrAxisNew);
					uint64 maxMomentum = m_axisOld.m_occurrences->Len() + m_axisNew.m_occurrences->Len();
					uint64 maxQuality = 1 + (maxMatches * params.m_quality_match) + (maxMomentum * params.m_quality_momentum);
					return maxQuality;
				}
			};


			struct CellDir { enum { End, Diag, Right, Down }; };

			struct Cell32
			{
				uint32 m_dir     :  2;
				uint32 m_quality : 30;
			};

			enum : uint32 { Cell32_MaxQuality = (1U << 30) - 1U };

			struct Cell64
			{
				uint64 m_dir     :  2;
				uint64 m_quality : 62;
			};

			static_assert(sizeof(Cell32) == sizeof(uint32), "Unexpected Cell32 size");
			static_assert(sizeof(Cell64) == sizeof(uint64), "Unexpected Cell64 size");


			template <class Cell>
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
						Debug_DumpMatrix(params);

					Travel(axes, diff, params);
				}
			
			private:
				MatrixAxis const* m_rowAxis {};
				MatrixAxis const* m_colAxis {};

				bool IsRowColUnitMatch(sizet row, sizet col) { return m_rowAxis->m_axisUnits[row].m_uu == m_colAxis->m_axisUnits[col].m_uu; }
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
					if (axes.m_axisNew.m_axisUnits.Len() < axes.m_axisOld.m_axisUnits.Len())
					     { m_colsNew = true;  m_colAxis = &axes.m_axisNew; m_rowAxis = &axes.m_axisOld; }
					else { m_colsNew = false; m_colAxis = &axes.m_axisOld; m_rowAxis = &axes.m_axisNew; }

					m_nrCols = m_colAxis->m_axisUnits.Len() + 1;	// Additional column for right edge
					m_nrRows = m_rowAxis->m_axisUnits.Len() + 1;	// Additional row for bottom edge

					m_width = m_nrCols;

					enum : uint { MinMatrixWidth = 2U };
					uint const maxMatrixWidth = PickMax<uint>(MinMatrixWidth, params.m_maxMatrixWidth);
					if (m_width > maxMatrixWidth)
						m_width = maxMatrixWidth;

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

								if (IsRowColUnitMatch(row, col))
								{
									diag = GetCell(row+1, col+1);
									diagQuality = params.m_quality_match + MoveQualityAndMomentum(row, col, CellDir::Diag, diag, params);
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

				void Debug_DumpMatrix(DiffParams const& params)
				{
					if (!params.m_debugHtml)
						return;

					HtmlBuilder& html = *params.m_debugHtml;
					html.H2().T("Horizontal axis ").T(m_colsNew ? "(new)" : "(old)").EndH2();
					m_colAxis->Debug_DumpAxis(html);

					html.H2().T("Vertical axis ").T(m_colsNew ? "(old)" : "(new)").EndH2();
					m_rowAxis->Debug_DumpAxis(html);

					html.H2().T("Matrix").EndH2()
						.Table().Class("AtDiff")
							.Tr()
								.Th().EndTh();

					for (sizet col=0; col!=m_nrCols; ++col)
						html	.Th().UInt(col).EndTh();
								
					html	.EndTr();

					sizet hlRow {}, hlCol {};
					for (sizet row=0; row!=m_nrRows; ++row)
					{
						html.Tr()
								.Th().UInt(row).EndTh();

						for (sizet col=0; col!=m_nrCols; ++col)
						{
							bool hl = (col == hlCol && row == hlRow);
							html.Td();
							if (hl) html.Class("hl");
							Cell* cell = GetCell(row, col);
							if (cell)
							{
								html.UInt(cell->m_quality).Br();

								switch (cell->m_dir)
								{
								case CellDir::Right: html.T("&rarr;"); if (hl) ++hlCol; break;
								case CellDir::Down:  html.T("&darr;"); if (hl) ++hlRow; break;
								case CellDir::Diag:  html.T("&searr;"); if (hl) { ++hlCol; ++hlRow; } break;
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

		}	// Internal

		using namespace Internal;


		void DebugCss(HtmlBuilder& html)
		{
			html.AddCss("body "               "{ font-size: x-small } "
						"table.AtDiff "       "{ border-collapse: collapse } "
						"table.AtDiff th "    "{ border: 1px solid #666; background: #eee; font-weight: normal; padding: 0px 5px 0px 5px; } "
						"table.AtDiff td "    "{ border: 1px solid #666; text-align: center; padding: 0px 5px 0px 5px; } "
						"table.AtDiff td.hl " "{ background: #def; } ");
		}


		void Generate(PtrPair<InputUnit> inputOld, PtrPair<InputUnit> inputNew, Vec<DiffUnit>& diff, DiffParams const& params)
		{
			diff.ReserveExact(inputOld.Len() + inputNew.Len());

			TrivialDiff(inputOld, inputNew, diff, params);
			EnsureThrow(inputOld.Any() == inputNew.Any());

			if (inputOld.Any())
			{
				PtrPair<InputUnit> commonTailNew;
				CutCommonTail(inputOld, inputNew, commonTailNew);

				UniqueUnitView uuv;
				uuv.Build(inputOld, inputNew);

				MatrixAxes axes;
				axes.Build(uuv);

				if (axes.MaxPossibleCellQuality(params) > Cell32_MaxQuality)
				{
					SlidingMatrix<Cell64> matrix;
					matrix.BuildAndSolve(axes, diff, params);
				}
				else
				{
					SlidingMatrix<Cell32> matrix;
					matrix.BuildAndSolve(axes, diff, params);
				}

				DiffCommonTail(commonTailNew, diff, params);
			}
		}

	}
}
