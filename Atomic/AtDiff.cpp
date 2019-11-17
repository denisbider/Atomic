#include "AtIncludes.h"
#include "AtDiff.h"

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
				bool m_isInOld {};
				bool m_isInNew {};

				UniqueUnit(Seq value) : m_value(value) {}

				bool IsAxisUnit() const { return m_isInOld && m_isInNew; }
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

				void Build(PtrPair<InputUnit> inputOld, PtrPair<InputUnit> inputNew)
				{
					m_uniqueUnitStorage.FixReserve(inputOld.Len() + inputNew.Len());
					m_occurrencesOld.FixReserve(inputOld.Len());
					m_occurrencesNew.FixReserve(inputNew.Len());

					for (sizet i=0; i!=inputOld.Len(); ++i)
					{
						InputUnit const& iu = inputOld[i];
						UniqueUnit* uu = FindOrAddUniqueUnit(iu);
						uu->m_isInOld = true;
						m_occurrencesOld.Add(&iu, uu);
					}

					for (sizet i=0; i!=inputNew.Len(); ++i)
					{
						InputUnit const& iu = inputNew[i];
						UniqueUnit* uu = FindOrAddUniqueUnit(iu);
						uu->m_isInNew = true;
						m_occurrencesNew.Add(&iu, uu);
					}
				}

			private:
				Vec<UniqueUnit> m_uniqueUnitStorage;

				UniqueUnit* FindOrAddUniqueUnit(InputUnit const& iu)
				{
					bool added {};
					UniqueUnitMapEntry entry { iu.m_value };
					UniqueUnitMap::It it = m_uum.FindOrAdd(added, std::move(entry));
					if (added)
						it->m_uniqueUnit = &m_uniqueUnitStorage.Add(iu.m_value);
					return it->m_uniqueUnit;
				}
			};


			struct AxisUnit
			{
				// An AxisUnit consists of:
				// - zero or more Vec<Occurrence> positions corresponding to units that appear only on this axis, followed by
				// - the Vec<Occurrence> position of exactly one unit which appears on both axes.
				//
				// The units that appear only on one axis are irrelevant to finding the longest common subsequence,
				// so they can be trivially bundled with the next unit on the axis.

				sizet       m_leadPos {};
				sizet       m_unitPos {};
				UniqueUnit* m_uu      {};

				AxisUnit(sizet leadPos, sizet unitPos, UniqueUnit* uu)
					: m_leadPos(leadPos), m_unitPos(unitPos), m_uu(uu) {}
			};


			struct Axis
			{
				Vec<Occurrence> const* m_occurrences;
				Vec<AxisUnit>          m_axisUnits;
				sizet                  m_trailPos {};

				bool  Any() const { return m_axisUnits.Any(); }
				sizet Len() const { return m_axisUnits.Len(); }

				void Build(Vec<Occurrence> const& occurrences)
				{
					m_occurrences = &occurrences;

					sizet nrAxisUnits = CountNrAxisUnits(occurrences);
					if (nrAxisUnits)
					{
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

						m_trailPos = leadPos;
					}
				}

			private:
				static sizet CountNrAxisUnits(Vec<Occurrence> const& occurrences)
				{
					sizet count {};
					for (Occurrence const& o : occurrences)
						if (o.m_uu->IsAxisUnit())
							++count;
					return count;
				}
			};


			struct Diagonalizer
			{
				Axis m_xAxis;	// Old input (deletions and snakes)
				Axis m_yAxis;	// New input (insertions and snakes)

				void Build(UniqueUnitView const& uuv)
				{
					m_xAxis.Build(uuv.m_occurrencesOld);
					m_yAxis.Build(uuv.m_occurrencesNew);

					EnsureThrow(m_xAxis.Any() == m_yAxis.Any());
					if (m_xAxis.Any())
					{
						m_nrDiags = m_xAxis.Len() + m_yAxis.Len() + 1;
						m_diagMin = -(NumCast<ptrdiff>(m_yAxis.Len()));

						// Set maximum number of steps to approximate square root of input size, but not less than 4000
						m_maxNrSteps = 1;
						for (sizet v=m_nrDiags; v; v>>=2)
							m_maxNrSteps <<= 1;
						if (m_maxNrSteps < 4000)
							m_maxNrSteps = 4000;

						m_diagsFwd.Resize(m_nrDiags);
						m_diagsBwd.Resize(m_nrDiags);
					}
				}

				void Process(Vec<DiffUnit>& diff, DiffParams const& params)
				{
					if (m_xAxis.Any())
					{
						Vec<Section> sections;
						{
							Coord ofs { 0, 0 };
							Coord end { NumCast<ptrdiff>(m_xAxis.Len()), NumCast<ptrdiff>(m_yAxis.Len()) };
							sections.Add(ofs, ofs, end, end, params.m_limitSteps);
						}

						do
						{
							Section& s = sections.First();

							// Skip leading snake, if any
							while (s.ofs.x != s.end.x && s.ofs.y != s.end.y && UU_X(s.ofs.x) == UU_Y(s.ofs.y))
								{ ++(s.ofs.x); ++(s.ofs.y); }

							// Skip trailing snake, if any
							while (s.end.x != s.ofs.x && s.end.y != s.ofs.y && UU_X(s.end.x - 1) == UU_Y(s.end.y - 1))
								{ --(s.end.x); --(s.end.y); }

							// Trivial deletion?
							if (s.ofs.y == s.end.y)
							{
								EmitLeadSnake(diff, s);

								while (s.ofs.x != s.end.x)
								{
									AxisUnit const& au = m_xAxis.m_axisUnits[(sizet) s.ofs.x];
									for (sizet pos=au.m_leadPos; pos<=au.m_unitPos; ++pos)
										diff.Add(DiffDisposition::Removed, *(*m_xAxis.m_occurrences)[pos].m_iu);
									++s.ofs.x;
								}

								EmitTrailSnake(diff, s);
								sections.Erase(0, 1);
							}

							// Trivial insertion?
							else if (s.ofs.x == s.end.x)
							{
								EmitLeadSnake(diff, s);

								while (s.ofs.y != s.end.y)
								{
									AxisUnit const& au = m_yAxis.m_axisUnits[(sizet) s.ofs.y];
									for (sizet pos=au.m_leadPos; pos<=au.m_unitPos; ++pos)
										diff.Add(DiffDisposition::Added, *(*m_yAxis.m_occurrences)[pos].m_iu);
									++s.ofs.y;
								}

								EmitTrailSnake(diff, s);
								sections.Erase(0, 1);
							}

							else
							{
								SplitPoint split;
								FindSplitPoint(s, split);

								Coord end = s.end, trailSnakeEnd = s.trailSnakeEnd;
								s = Section(s.leadSnakeOfs, s.ofs, split.ofs, split.ofs, split.limitSteps_left);
								sections.Insert(1, split.ofs, split.ofs, end, trailSnakeEnd, split.limitSteps_right);
							}
						}
						while (sections.Any());
					}

					// Add trailing occurrences that aren't part of either axis, so therefore are different
					for (sizet pos=m_xAxis.m_trailPos; pos!=m_xAxis.m_occurrences->Len(); ++pos)
						diff.Add(DiffDisposition::Removed, *(*m_xAxis.m_occurrences)[pos].m_iu);

					for (sizet pos=m_yAxis.m_trailPos; pos!=m_yAxis.m_occurrences->Len(); ++pos)
						diff.Add(DiffDisposition::Added, *(*m_yAxis.m_occurrences)[pos].m_iu);
				}

			private:
				sizet        m_nrDiags {};
				ptrdiff      m_diagMin {};
				sizet        m_maxNrSteps {};
				Vec<ptrdiff> m_diagsFwd;
				Vec<ptrdiff> m_diagsBwd;

				ptrdiff& FD_X(ptrdiff diag) { return m_diagsFwd[(sizet) (diag - m_diagMin)]; }
				ptrdiff& BD_X(ptrdiff diag) { return m_diagsBwd[(sizet) (diag - m_diagMin)]; }
				static ptrdiff Diag(ptrdiff x, ptrdiff y) { return x - y; }

				UniqueUnit const* UU_X(ptrdiff x) const { return m_xAxis.m_axisUnits[(sizet) x].m_uu; }
				UniqueUnit const* UU_Y(ptrdiff y) const { return m_yAxis.m_axisUnits[(sizet) y].m_uu; }

				struct Coord
				{
					ptrdiff x {};
					ptrdiff y {};

					Coord() {}
					Coord(ptrdiff x_, ptrdiff y_) : x(x_), y(y_)
						{ EnsureThrow(x >= 0 && y >= 0); }
				};

				struct Section
				{
					Coord leadSnakeOfs;
					Coord ofs;
					Coord end;
					Coord trailSnakeEnd;
					bool  limitSteps {};

					Section(Coord leadSnakeOfs_, Coord ofs_, Coord end_, Coord trailSnakeEnd_, bool limitSteps_)
						: leadSnakeOfs(leadSnakeOfs_), ofs(ofs_), end(end_), trailSnakeEnd(trailSnakeEnd_), limitSteps(limitSteps_)
					{
						EnsureThrow(ofs.x >= leadSnakeOfs.x);
						EnsureThrow(ofs.y - leadSnakeOfs.y == ofs.x - leadSnakeOfs.x);
						EnsureThrow(end.x >= ofs.x);
						EnsureThrow(end.y >= ofs.y);
						EnsureThrow((end.x - ofs.x) + (end.y - ofs.y) >= 1);
						EnsureThrow(trailSnakeEnd.x >= end.x);
						EnsureThrow(trailSnakeEnd.y - end.y == trailSnakeEnd.x - end.x);
					}
				};

				void EmitLeadSnake  (Vec<DiffUnit>& diff, Section s) { EmitSnake(diff, s.leadSnakeOfs, s.ofs           ); }
				void EmitTrailSnake (Vec<DiffUnit>& diff, Section s) { EmitSnake(diff, s.end,          s.trailSnakeEnd ); }

				void EmitSnake(Vec<DiffUnit>& diff, Coord ofs, Coord end)
				{
					ptrdiff x = ofs.x, y = ofs.y;
					for (; x!=end.x; ++x, ++y)
					{
						AxisUnit const& aux = m_xAxis.m_axisUnits[(sizet) x];
						for (sizet pos=aux.m_leadPos; pos!=aux.m_unitPos; ++pos)
							diff.Add(DiffDisposition::Removed, *(*m_xAxis.m_occurrences)[pos].m_iu);

						AxisUnit const& auy = m_yAxis.m_axisUnits[(sizet) y];
						for (sizet pos=auy.m_leadPos; pos!=auy.m_unitPos; ++pos)
							diff.Add(DiffDisposition::Added, *(*m_yAxis.m_occurrences)[pos].m_iu);
					}
					EnsureThrow(y == end.y);
				}

				struct SplitPoint
				{
					Coord ofs;
					bool limitSteps_left  {};
					bool limitSteps_right {};

					void Set(Coord ofs_, bool lsLeft, bool lsRight) { ofs = ofs_; limitSteps_left = lsLeft; limitSteps_right = lsRight; }
				};

				void FindSplitPoint(Section s, SplitPoint& split)
				{
					ptrdiff const dMin = Diag(s.ofs.x, s.end.y);
					ptrdiff const dMax = Diag(s.end.x, s.ofs.y);
					ptrdiff const fMid = Diag(s.ofs.x, s.ofs.y);
					ptrdiff const bMid = Diag(s.end.x, s.end.y);
					bool odd = (0 != ((fMid - bMid) & 1));

					FD_X(fMid) = s.ofs.x;
					BD_X(bMid) = s.end.x;

					ptrdiff fMin = fMid, fMax = fMid;
					ptrdiff bMin = bMid, bMax = bMid;

					for (sizet stepNr=1; !s.limitSteps || stepNr!=m_maxNrSteps; ++stepNr)
					{
						// Make an edit step forward
						if (fMin > dMin)
							FD_X(--fMin - 1) = -1;
						else
							++fMin;

						if (fMax < dMax)
							FD_X(++fMax + 1) = -1;
						else
							--fMax;

						for (ptrdiff diag = fMax; diag >= fMin; diag -= 2)
						{
							ptrdiff tLo = FD_X(diag - 1);
							ptrdiff tHi = FD_X(diag + 1);
							ptrdiff xStart = tLo < tHi ? tHi : tLo + 1;

							ptrdiff x = xStart, y = xStart - diag;
							while (x < s.end.x && y < s.end.y && UU_X(x) == UU_Y(y))
								{ ++x; ++y; }

							FD_X(diag) = x;

							if (odd && diag >= bMin && diag <= bMax && BD_X(diag) <= x)
							{
								split.Set(Coord(x, y), false, false);
								return;
							}
						}

						// Make an edit step backward
						if (bMin > dMin)
							BD_X(--bMin - 1) = PTRDIFF_MAX;
						else
							++bMin;

						if (bMax < dMax)
							BD_X(++bMax + 1) = PTRDIFF_MAX;
						else
							--bMax;

						for (ptrdiff diag = bMax; diag >= bMin; diag -= 2)
						{
							ptrdiff tLo = BD_X(diag - 1);
							ptrdiff tHi = BD_X(diag + 1);
							ptrdiff xStart = tLo < tHi ? tLo : tHi - 1;

							ptrdiff x = xStart, y = xStart - diag;
							while (x > s.ofs.x && y > s.ofs.y && UU_X(x-1) == UU_Y(y-1))
								{ --x; --y; }

							BD_X(diag) = x;

							if (!odd && diag >= fMin && diag <= fMax && x <= FD_X(diag))
							{
								split.Set(Coord(x, y), false, false);
								return;
							}
						}
					}

					// Number of steps exceeded. Return the best we have
					
					// Find a forward diagonal with maximal x + y
					ptrdiff fMaxXY = -1, fBestX {};
					for (ptrdiff diag = fMax; diag >= fMin; diag -= 2)
					{
						ptrdiff x = PickMin(FD_X(diag), s.end.x);
						ptrdiff y = x - diag;
						
						if (y > s.end.y)
						{
							x = s.end.y + diag;
							y = s.end.y;
						}

						if (fMaxXY < x + y)
						{
							fMaxXY = x + y;
							fBestX = x;
						}
					}

					// Find a backward diagonal with minimal x + y
					ptrdiff bMinXY = PTRDIFF_MAX, bBestX {};
					for (ptrdiff diag = bMax; diag >= bMin; diag -= 2)
					{
						ptrdiff x = PickMax(BD_X(diag), s.ofs.x);
						ptrdiff y = x - diag;

						if (y < s.ofs.y)
						{
							x = s.ofs.y + diag;
							y = s.ofs.y;
						}

						if (bMinXY > x + y)
						{
							bMinXY = x + y;
							bBestX = x;
						}
					}

					// Which of the two diagonals is best?
					ptrdiff ofsXY = s.ofs.x + s.ofs.y;
					ptrdiff endXY = s.end.x + s.end.y;

					if (fMaxXY - ofsXY > endXY - bMinXY)
						split.Set(Coord(fBestX, fMaxXY - fBestX), false, true);
					else
						split.Set(Coord(bBestX, bMinXY - bBestX), true, false);
				}
			};


			void CutCommonTail(PtrPair<InputUnit>& inputOld, PtrPair<InputUnit>& inputNew)
			{
				while (inputOld.Any() && inputNew.Any())
				{
					if (inputOld.Last().m_value != inputNew.Last().m_value)
						break;

					inputOld.PopLast();
					inputNew.PopLast();
				}
			}

			
			void TrivialDiff(PtrPair<InputUnit>& inputOld, PtrPair<InputUnit>& inputNew, Vec<DiffUnit>& diff)
			{
				while (inputOld.Any() && inputNew.Any())
				{
					if (inputOld.First().m_value != inputNew.First().m_value)
						return;

					inputOld.PopFirst();
					inputNew.PopFirst();
				}

				while (inputOld.Any())
				{
					diff.Add(DiffDisposition::Removed, inputOld.First());
					inputOld.PopFirst();
				}

				while (inputNew.Any())
				{
					diff.Add(DiffDisposition::Added, inputNew.First());
					inputNew.PopFirst();
				}
			}

		}	// Internal

		using namespace Internal;


		void Generate(PtrPair<InputUnit> inputOld, PtrPair<InputUnit> inputNew, Vec<DiffUnit>& diff, DiffParams const& params)
		{
			diff.ReserveExact(inputOld.Len() + inputNew.Len());

			CutCommonTail(inputOld, inputNew);
			TrivialDiff(inputOld, inputNew, diff);
			EnsureThrow(inputOld.Any() == inputNew.Any());

			if (inputOld.Any())
			{
				UniqueUnitView uuv;
				uuv.Build(inputOld, inputNew);

				Diagonalizer diag;
				diag.Build(uuv);
				diag.Process(diff, params);
			}
		}

	}
}
