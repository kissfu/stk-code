//  $Id: main_loop.cpp 855 2006-11-17 01:50:37Z coz $
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004 Ingo Ruhnke <grumbel@gmx.de>
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "main_loop.hpp"

#include <assert.h>

#include "audio/sound_manager.hpp"
#include "config/user_config.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/material_manager.hpp"
#include "guiengine/engine.hpp"
#include "input/input_manager.hpp"
#include "modes/profile_world.hpp"
#include "modes/world.hpp"
#include "network/network_manager.hpp"
#include "race/history.hpp"
#include "race/race_manager.hpp"
#include "states_screens/state_manager.hpp"

MainLoop* main_loop = 0;

// FIXME hacky hacky FPS info
int minFPS  = 999;
int lastFPS =  -1;
int maxFPS  =   0;

MainLoop::MainLoop() :
m_abort(false),
m_frame_count(0)
{
    m_curr_time = 0;
    m_prev_time = 0;
}  // MainLoop

//-----------------------------------------------------------------------------
MainLoop::~MainLoop()
{
}   // ~MainLoop

//-----------------------------------------------------------------------------
/** Returns the current dt, which guarantees a limited frame rate. If dt is
 *  too low (the frame rate too high), the process will sleep to reach the
 *  maxium frame rate.
 */
float MainLoop::getLimitedDt()
{
    IrrlichtDevice* device = irr_driver->getDevice();
    m_prev_time = m_curr_time;
    
    float dt;  // needed outside of the while loop
    while( 1 )
    {
        m_curr_time = device->getTimer()->getRealTime();
        dt = (float)(m_curr_time - m_prev_time);

        // don't allow the game to run slower than a certain amount.
        // when the computer can't keep it up, slow down the shown time instead
        static const float max_elapsed_time = 3.0f*1.0f/60.0f*1000.0f; /* time 3 internal substeps take */
        if(dt > max_elapsed_time) dt=max_elapsed_time;

        // Throttle fps if more than maximum, which can reduce 
        // the noise the fan on a graphics card makes.
        // When in menus, reduce FPS much, it's not necessary to push to the maximum for plain menus
        const int max_fps = (StateManager::get()->getGameState() == GUIEngine::GAME ? UserConfigParams::m_max_fps : 35);
        const int current_fps = (int)(1000.0f/dt);
        if( current_fps > max_fps )
        {
            int wait_time = 1000/max_fps - 1000/current_fps;
            if(wait_time < 1) wait_time = 1;

            irr_driver->getDevice()->sleep(wait_time);
        }
        else break;
    }
    dt *= 0.001f;
    return dt;
}

//-----------------------------------------------------------------------------
/** Updates all race related objects.
 *  \param dt Time step size.
 */
void MainLoop::updateRace(float dt)
{
    // Server: Send the current position and previous controls to all clients
    // Client: send current controls to server
    // But don't do this if the race is in finish phase (otherwise 
    // messages can be mixed up in the race manager)
    if(!race_manager->getWorld()->isFinishPhase())
        network_manager->sendUpdates();
    if(ProfileWorld::isProfileMode()) dt=1.0f/60.0f;

    // Again, only receive updates if the race isn't over - once the
    // race results are displayed (i.e. game is in finish phase) 
    // messages must be handled by the normal update of the network 
    // manager
    if(!race_manager->getWorld()->isFinishPhase())
        network_manager->receiveUpdates();

    if ( RaceManager::getWorld()->getPhase() != LIMBO_PHASE)
    {
        history->update(dt);
        RaceManager::getWorld()->update(dt);
    }   // phase != limbo phase
}   // updateRace

//-----------------------------------------------------------------------------
/** Run the actual main loop.
 */
void MainLoop::run()
{
    IrrlichtDevice* device = irr_driver->getDevice();
    
    bool music_on = false;
    m_curr_time = device->getTimer()->getRealTime();
    while(!m_abort)
    {
        m_prev_time = m_curr_time;
        float dt   = getLimitedDt();

        if (!music_on && !race_manager->raceIsActive())
        {
            sound_manager->stopMusic();   // stop potential 'left over' music from race
            sound_manager->startMusic(stk_config->m_title_music);
            music_on = true;
        }
        network_manager->update(dt);

        if (race_manager->raceIsActive())
        {
            // Busy wait if race_manager is active (i.e. creating of world is done)
            // till all clients have reached this state.
            if (network_manager->getState()==NetworkManager::NS_READY_SET_GO_BARRIER) continue;
            updateRace(dt);
            music_on = false; 
        }   // if race is active

        sound_manager->update(dt);
        input_manager->update(dt);
        irr_driver->update(dt);
    }  // while !m_exit

}   // run

//-----------------------------------------------------------------------------
/** Set the abort flag, causing the mainloop to be left.
 */
void MainLoop::abort()
{
    m_abort = true;
}   // abort

/* EOF */
