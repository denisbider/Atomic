#include "AtIncludes.h"
#include "AtEnsureStackTrace.h"

#include "AtEnsureConvert.h"


namespace At
{

	namespace Internal
	{

		// ModuleInfo

		struct ModuleInfo : NoCopy
		{
			HMODULE    m_hModule {};
			MODULEINFO m_info;

			~ModuleInfo() noexcept;
			bool InitFromAddress(EnsureFailDescRef& efdRef, HANDLE hProcess, void* addr) noexcept;
			bool IsSameAs(ModuleInfo const& x) const noexcept { return !!m_hModule && !!x.m_hModule && m_info.lpBaseOfDll == x.m_info.lpBaseOfDll; }
			void MoveFrom(ModuleInfo&& x) noexcept;
		};


		ModuleInfo::~ModuleInfo() noexcept
		{
			if (m_hModule)
				FreeLibrary(m_hModule);
		}


		bool ModuleInfo::InitFromAddress(EnsureFailDescRef& efdRef, HANDLE hProcess, void* addr) noexcept
		{
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) addr, &m_hModule))
			{
				DWORD err = GetLastError();
				efdRef.Add(__FUNCTION__ ": Error in GetModuleHandleExW: ").UIntMaybeHex(err).Add("\r\n");

				m_hModule = 0;
				return false;
			}

			if (!GetModuleInformation(hProcess, m_hModule, &m_info, sizeof(m_info)))
			{
				DWORD err = GetLastError();
				efdRef.Add(__FUNCTION__ ": Error in GetModuleInformation: ").UIntMaybeHex(err).Add("\r\n");

				FreeLibrary(m_hModule);
				m_hModule = 0;
				return false;
			}

			return true;
		}


		void ModuleInfo::MoveFrom(ModuleInfo&& x) noexcept
		{
			if (m_hModule)
				FreeLibrary(m_hModule);

			m_hModule = x.m_hModule;
			x.m_hModule = 0;

			memcpy(&m_info, &x.m_info, sizeof(m_info));
		}



		// StackFrameDescriber

		class StackFrameDescriber : NoCopy
		{
		public:
			~StackFrameDescriber() noexcept { LeaveCriticalSection(&s_cs); }

			void Init(EnsureFailDescRef& efdRef) noexcept;
			void AddStackFrameDesc(EnsureFailDescRef& efdRef, void* stackFramePtr) noexcept;

		private:
			struct CritSect : CRITICAL_SECTION, NoCopy
			{
				CritSect() noexcept { InitializeCriticalSection(this); }
				~CritSect() noexcept { DeleteCriticalSection(this); }
			};

			static CritSect s_cs;

			HANDLE     m_hProcess {};
			ModuleInfo m_mi;
			wchar_t    m_moduleNameBuf[MAX_PATH];
			wchar_t*   m_moduleNameStart {};
			bool       m_haveModuleName  {};

			EnsureConvert_WtoA m_moduleNameA;

		#ifdef _DEBUG
			struct SymInitializer : NoCopy
			{
				enum class State { None, Inited, Failed };
				HANDLE m_hProcess {};
				State m_state     { State::None };
				DWORD m_initErr   {};

				~SymInitializer() noexcept
				{
					if (Inited())
						SymCleanup(m_hProcess);
				}

				bool Inited() const noexcept { return m_state == State::Inited; }

				bool Init(HANDLE hProcess) noexcept
				{
					if (m_state == State::None)
					{
						if (!SymInitialize(hProcess, nullptr, false))
						{
							m_initErr = GetLastError();
							m_state = State::Failed;
						}
						else
						{
							m_hProcess = hProcess;
							m_state = State::Inited;

							SymSetOptions(SYMOPT_LOAD_LINES);
						}
					}

					return Inited();
				}
			};

			static SymInitializer s_symInitializer;

			static bool IsKnownSystemModule(char const* z);
		#endif
		};


		StackFrameDescriber::CritSect StackFrameDescriber::s_cs;

	#ifdef _DEBUG
		StackFrameDescriber::SymInitializer StackFrameDescriber::s_symInitializer;

		bool StackFrameDescriber::IsKnownSystemModule(char const* z)
		{
			enum { NrModules = 3 };
			char const* const c_modules[NrModules] = { "kernel32.dll", "kernelbase.dll", "ntdll.dll" };
			for (int i=0; i!=NrModules; ++i)
				if (!_stricmp(z, c_modules[i]))
					return true;
			return false;
		}
	#endif


		void StackFrameDescriber::Init(EnsureFailDescRef& efdRef) noexcept
		{
			EnterCriticalSection(&s_cs);
			m_hProcess = GetCurrentProcess();

		#ifndef _DEBUG
			(void) efdRef;
		#else
			if (!s_symInitializer.Init(m_hProcess))
				efdRef.Add(__FUNCTION__ ": Error in SymInitialize: ").UIntMaybeHex(s_symInitializer.m_initErr).Add("\r\n");
		#endif
		}


		void StackFrameDescriber::AddStackFrameDesc(EnsureFailDescRef& efdRef, void* stackFramePtr) noexcept
		{
			bool haveModuleInfo {}, loadModuleName {};

			{
				ModuleInfo mi;
				if (mi.InitFromAddress(efdRef, m_hProcess, stackFramePtr))
				{
					haveModuleInfo = true;

					if (!m_mi.IsSameAs(mi))
					{
						m_mi.MoveFrom(std::move(mi));
						loadModuleName = true;
					}
				}
			}

			if (!haveModuleInfo)
				efdRef.Add("?noModuleInfo!").UIntMaybeHex((sizet) stackFramePtr).Add("\r\n");
			else
			{
				sizet moduleBase = (sizet) m_mi.m_info.lpBaseOfDll;

				if (loadModuleName)
				{
					m_haveModuleName = false;
					m_moduleNameStart = m_moduleNameBuf;
					DWORD nameLen = GetModuleFileNameW(m_mi.m_hModule, m_moduleNameBuf, sizeof(m_moduleNameBuf));
					if (!nameLen || nameLen >= sizeof(m_moduleNameBuf))
					{
						DWORD err = GetLastError();
						efdRef.Add(__FUNCTION__ ": Error in GetModuleFileName: ").UIntMaybeHex(err)
							.Add(", module base: ").UIntHex(moduleBase).Add("\r\n");

						wcscpy_s<MAX_PATH>(m_moduleNameBuf, L"?no-module-name");
					}
					else
					{
						sizet i = nameLen;
						for (; i; --i)
							if (m_moduleNameBuf[i-1] == '\\')
								break;

						m_moduleNameStart += i;
						m_haveModuleName = true;
					}

					m_moduleNameA.Convert(m_moduleNameStart, CP_UTF8);
				}

				sizet relAddr = ((sizet) stackFramePtr) - moduleBase;
				efdRef.Add(m_moduleNameA.Z()).Add("!").UIntHex(relAddr);

			#ifdef _DEBUG
				if (m_haveModuleName && s_symInitializer.Inited() && !IsKnownSystemModule(m_moduleNameA.Z()))
				{
					bool found {};
					DWORD displacement {};
					IMAGEHLP_LINEW64 line { 0 };
					line.SizeOfStruct = sizeof(line);

					DWORD64 addr64 = (sizet) stackFramePtr;	// Avoid sign-extension
					if (SymGetLineFromAddrW64(m_hProcess, addr64, &displacement, &line))
						found = true;
					else
					{
						DWORD err = GetLastError();
						if (err != ERROR_MOD_NOT_FOUND)
							efdRef.Add(" - error in SymGetLineFromAddr64: ").UIntMaybeHex(err);
						else
						{
							if (!SymLoadModuleExW(m_hProcess, nullptr, (PCWSTR) m_moduleNameStart, nullptr, moduleBase, m_mi.m_info.SizeOfImage, nullptr, 0))
							{
								err = GetLastError();
								efdRef.Add(" - error in SymLoadModuleEx: ").UIntMaybeHex(err);
							}
							else if (!SymGetLineFromAddrW64(m_hProcess, addr64, &displacement, &line))
							{
								err = GetLastError();
								efdRef.Add(" - error in SymGetLineFromAddr64: ").UIntMaybeHex(err);
							}
						}
					}

					if (found)
					{
						EnsureConvert_WtoA fname { line.FileName, CP_UTF8 };
						efdRef.Add(" - ").Add(fname.Z()).Add(":").UInt(line.LineNumber);
						if (displacement)
							efdRef.Add(" +").UIntMaybeHex(displacement).Add("b");
					}
				}
			#endif
				
				efdRef.Add("\r\n");
			}
		}

	}

	using namespace Internal;



	// Ensure_AddStackTrace

	void Ensure_AddStackTrace(EnsureFailDescRef& efdRef, ULONG framesToSkip, ULONG framesToCapture) noexcept
	{
		enum {	MinFramesToCapture = 5,
				MaxFramesCombined  = 62,
				MaxFramesToSkip    = MaxFramesCombined - MinFramesToCapture };

		void* frames[MaxFramesCombined];
		if (framesToSkip > MaxFramesToSkip)
			framesToSkip = MaxFramesToSkip;
		if (framesToCapture > MaxFramesCombined - framesToSkip)
			framesToCapture = MaxFramesCombined - framesToSkip;

		WORD nrFrames = CaptureStackBackTrace(framesToSkip, framesToCapture, frames, nullptr);
		if (!nrFrames)
			efdRef.Add("CaptureStackBackTrace returned no frames\r\n");
		else if (nrFrames > framesToCapture)
			efdRef.Add("CaptureStackBackTrace returned an invalid number of frames\r\n");
		else
		{
			StackFrameDescriber describer;
			describer.Init(efdRef);
			for (uint i=0; i!=nrFrames; ++i)
				describer.AddStackFrameDesc(efdRef, frames[i]);
		}
	}



	// Ensure_AddExceptionContext

	void Ensure_AddExceptionContext(EnsureFailDescRef& efdRef, _EXCEPTION_POINTERS* p) noexcept
	{
		if (!p)
			efdRef.Add("Exception pointers not available\r\n");
		else if (!p->ContextRecord)
			efdRef.Add("Exception context record not available\r\n");
		else
		{
			StackFrameDescriber describer;
			describer.Init(efdRef);

		#ifdef _M_X64
			describer.AddStackFrameDesc(efdRef, (void*) (p->ContextRecord->Rip));
		#else
			describer.AddStackFrameDesc(efdRef, (void*) (p->ContextRecord->Eip));
		#endif
		}
	}

}
