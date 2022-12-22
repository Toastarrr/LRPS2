/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "App.h"
#include "MTVU.h" // for thread cancellation on shutdown

#include <memory>

bool Pcsx2App::DetectCpuAndUserMode(void)
{
#ifdef _M_X86
	x86caps.Identify();
	x86caps.SIMD_EstablishMXCSRmask();
#endif

	AppConfig_OnChangedSettingsFolder();

	return true;
}

void Pcsx2App::AllocateCoreStuffs(void)
{
	if( AppRpc_TryInvokeAsync( &Pcsx2App::AllocateCoreStuffs ) ) return;

	AppApplySettings();

	GetVmReserve().ReserveAll();

	if( !m_CpuProviders )
	{
		// FIXME : Some or all of SysCpuProviderPack should be run from the SysExecutor thread,
		// so that the thread is safely blocked from being able to start emulation.

		m_CpuProviders = std::make_unique<SysCpuProviderPack>();

		if( m_CpuProviders->HadSomeFailures( g_Conf->EmuOptions.Cpu.Recompiler ) )
		{
			// HadSomeFailures only returns 'true' if an *enabled* cpu type fails to init.  If
			// the user already has all interps configured, for example, then no point in
			// popping up this dialog.
			Pcsx2Config::RecompilerOptions& recOps = g_Conf->EmuOptions.Cpu.Recompiler;
			
			if( m_CpuProviders->GetException_EE() )
				recOps.EnableEE		= false;

			if( m_CpuProviders->GetException_IOP() )
				recOps.EnableIOP	= false;

			if( m_CpuProviders->GetException_MicroVU0() )
				recOps.EnableVU0	= false;

			if( m_CpuProviders->GetException_MicroVU1() )
				recOps.EnableVU1	= false;
		}
	}
}

bool Pcsx2App::OnInit(void)
{
    return true;
}

// This cleanup procedure can only be called when the App message pump is still active.
// OnExit() must use CleanupOnExit instead.
void Pcsx2App::CleanupRestartable(void)
{
	CoreThread.Cancel();
}

// This cleanup handler can be called from OnExit (it doesn't need a running message pump),
// but should not be called from the App destructor.  It's needed because wxWidgets doesn't
// always call OnExit(), so I had to make CleanupRestartable, and then encapsulate it here
// to be friendly to the OnExit scenario (no message pump).
void Pcsx2App::CleanupOnExit(void)
{
	try
	{
		CleanupRestartable();
		CleanupResources();
	}
	catch( Exception::CancelEvent& )		{ throw; }
	catch( Exception::RuntimeError& ex )
	{
		// Handle runtime errors gracefully during shutdown.  Mostly these are things
		// that we just don't care about by now, and just want to "get 'er done!" so
		// we can exit the app. ;)

		log_cb(RETRO_LOG_ERROR, "Runtime exception handled during CleanupOnExit.\n" );
	}

	// Notice: deleting the plugin manager (unloading plugins) here causes Lilypad to crash,
	// likely due to some pending message in the queue that references lilypad procs.
	// We don't need to unload plugins anyway tho -- shutdown is plenty safe enough for
	// closing out all the windows.  So just leave it be and let the plugins get unloaded
	// during the wxApp destructor. -- air
	
	// FIXME: performing a wxYield() here may fix that problem. -- air
}

void Pcsx2App::CleanupResources(void)
{
	m_mtx_LoadingGameDB.Wait();
	ScopedLock lock(m_mtx_Resources);
	m_Resources = NULL;
}

int Pcsx2App::OnExit(void)
{
	CleanupOnExit();
	return wxApp::OnExit();
}

Pcsx2App::Pcsx2App(void)
{
}

Pcsx2App::~Pcsx2App(void)
{
	try
	{
		vu1Thread.Cancel(wxTimeSpan(0, 0, 5, 0));	// Quick fix to kill lingering processes that end up waiting here forever
		//vu1Thread.Cancel();
	}
	DESTRUCTOR_CATCHALL
}

void Pcsx2App::CleanUp(void)
{
}
