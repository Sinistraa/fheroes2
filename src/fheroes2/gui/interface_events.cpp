/***************************************************************************
 *   Copyright (C) 2012 by Andrey Afletdinov <fheroes2@gmail.com>          *
 *                                                                         *
 *   Part of the Free Heroes2 Engine:                                      *
 *   http://sourceforge.net/projects/fheroes2                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <algorithm>
#include <cassert>
#include <vector>

#include "agg.h"
#include "audio.h"
#include "cursor.h"
#include "dialog_system_options.h"
#include "game.h"
#include "game_interface.h"
#include "game_io.h"
#include "game_over.h"
#include "heroes.h"
#include "kingdom.h"
#include "logging.h"
#include "m82.h"
#include "settings.h"
#include "system.h"
#include "text.h"
#include "tools.h"
#include "translations.h"
#include "world.h"

void Interface::Basic::CalculateHeroPath( Heroes * hero, int32_t destinationIdx ) const
{
    if ( ( hero == nullptr ) || hero->Modes( Heroes::GUARDIAN ) ) {
        return;
    }

    hero->SetMove( false );
    hero->calculatePath( destinationIdx );

    const Route::Path & path = hero->GetPath();

    if ( destinationIdx < 0 ) {
        destinationIdx = path.GetDestinationIndex();
    }

    if ( destinationIdx < 0 ) {
        return;
    }

    DEBUG_LOG( DBG_GAME, DBG_TRACE, hero->GetName() << ", distance: " << world.getDistance( *hero, destinationIdx ) << ", route: " << path.String() );

    gameArea.SetRedraw();

    const fheroes2::Point & mousePos = LocalEvent::Get().GetMouseCursor();
    if ( gameArea.GetROI() & mousePos ) {
        const int32_t cursorIndex = gameArea.GetValidTileIdFromPoint( mousePos );
        Cursor::Get().SetThemes( GetCursorTileIndex( cursorIndex ) );
    }

    Interface::Basic::Get().buttonsArea.Redraw();
}

void Interface::Basic::ShowPathOrStartMoveHero( Heroes * hero, s32 destinationIdx )
{
    if ( !hero || hero->Modes( Heroes::GUARDIAN ) )
        return;

    const Route::Path & path = hero->GetPath();

    // show path
    if ( path.GetDestinationIndex() != destinationIdx ) {
        CalculateHeroPath( hero, destinationIdx );
    }
    // start move
    else if ( path.isValid() && hero->MayStillMove( false, true ) ) {
        SetFocus( hero );
        RedrawFocus();

        hero->SetMove( true );
    }
}

void Interface::Basic::MoveHeroFromArrowKeys( Heroes & hero, int direct )
{
    const bool fromWater = hero.isShipMaster();
    if ( Maps::isValidDirection( hero.GetIndex(), direct ) ) {
        s32 dst = Maps::GetDirectionIndex( hero.GetIndex(), direct );
        const Maps::Tiles & tile = world.GetTiles( dst );
        bool allow = false;

        switch ( tile.GetObject() ) {
        case MP2::OBJ_BOAT:
        case MP2::OBJ_CASTLE:
        case MP2::OBJ_HEROES:
        case MP2::OBJ_MONSTER:
            allow = true;
            break;

        default:
            allow = ( tile.isPassableFrom( Direction::CENTER, fromWater, false, hero.GetColor() ) || MP2::isActionObject( tile.GetObject(), fromWater ) );
            break;
        }

        if ( allow )
            ShowPathOrStartMoveHero( &hero, dst );
    }
}

void Interface::Basic::EventNextHero( void )
{
    const Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );
    const KingdomHeroes & myHeroes = myKingdom.GetHeroes();

    if ( myHeroes.empty() )
        return;

    if ( GetFocusHeroes() ) {
        KingdomHeroes::const_iterator it = std::find( myHeroes.begin(), myHeroes.end(), GetFocusHeroes() );
        KingdomHeroes::const_iterator currentHero = it;
        do {
            ++it;
            if ( it == myHeroes.end() )
                it = myHeroes.begin();
            if ( ( *it )->MayStillMove( true, false ) ) {
                SetFocus( *it );
                CalculateHeroPath( *it, -1 );
                break;
            }
        } while ( it != currentHero );
    }
    else {
        for ( Heroes * hero : myHeroes ) {
            if ( hero->MayStillMove( true, false ) ) {
                SetFocus( hero );
                CalculateHeroPath( hero, -1 );
                break;
            }
        }
    }
    RedrawFocus();
}

void Interface::Basic::EventContinueMovement( void ) const
{
    Heroes * hero = GetFocusHeroes();

    if ( hero && hero->GetPath().isValid() && hero->MayStillMove( false, true ) ) {
        hero->SetMove( true );
    }
}

void Interface::Basic::EventKingdomInfo( void ) const
{
    Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );
    myKingdom.openOverviewDialog();

    iconsPanel.SetRedraw();
}

void Interface::Basic::EventCastSpell( void )
{
    Heroes * hero = GetFocusHeroes();

    if ( hero ) {
        SetRedraw( REDRAW_ALL );
        ResetFocus( GameFocus::HEROES );
        Redraw();

        const Spell spell = hero->OpenSpellBook( SpellBook::Filter::ADVN, true, nullptr );
        // apply cast spell
        if ( spell.isValid() ) {
            hero->ActionSpellCast( spell );
            iconsPanel.SetRedraw();
        }
    }
}

fheroes2::GameMode Interface::Basic::EventEndTurn() const
{
    const Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );

    if ( GetFocusHeroes() )
        GetFocusHeroes()->SetMove( false );

    if ( !myKingdom.HeroesMayStillMove()
         || Dialog::YES == Dialog::Message( "", _( "One or more heroes may still move, are you sure you want to end your turn?" ), Font::BIG, Dialog::YES | Dialog::NO ) )
        return fheroes2::GameMode::END_TURN;

    return fheroes2::GameMode::CANCEL;
}

fheroes2::GameMode Interface::Basic::EventAdventureDialog()
{
    switch ( Dialog::AdventureOptions( GameFocus::HEROES == GetFocusType() ) ) {
    case Dialog::WORLD:
        EventViewWorld();
        break;

    case Dialog::PUZZLE:
        EventPuzzleMaps();
        break;

    case Dialog::INFO:
        return EventGameInfo();

    case Dialog::DIG:
        return EventDigArtifact();

    default:
        break;
    }

    return fheroes2::GameMode::CANCEL;
}

void Interface::Basic::EventViewWorld()
{
    ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::OnlyVisible, *this );
}

fheroes2::GameMode Interface::Basic::EventFileDialog() const
{
    return Dialog::FileOptions();
}

void Interface::Basic::EventSystemDialog() const
{
    fheroes2::showSystemOptionsDialog();
}

fheroes2::GameMode Interface::Basic::EventExit()
{
    if ( Dialog::YES & Dialog::Message( "", _( "Are you sure you want to quit?" ), Font::BIG, Dialog::YES | Dialog::NO ) )
        return fheroes2::GameMode::QUIT_GAME;

    return fheroes2::GameMode::CANCEL;
}

void Interface::Basic::EventNextTown( void )
{
    Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );
    KingdomCastles & myCastles = myKingdom.GetCastles();

    if ( !myCastles.empty() ) {
        if ( GetFocusCastle() ) {
            KingdomCastles::const_iterator it = std::find( myCastles.begin(), myCastles.end(), GetFocusCastle() );
            ++it;
            if ( it == myCastles.end() )
                it = myCastles.begin();
            SetFocus( *it );
        }
        else
            ResetFocus( GameFocus::CASTLE );

        RedrawFocus();
    }
}

fheroes2::GameMode Interface::Basic::EventNewGame() const
{
    return Dialog::YES == Dialog::Message( "", _( "Are you sure you want to restart? (Your current game will be lost.)" ), Font::BIG, Dialog::YES | Dialog::NO )
               ? fheroes2::GameMode::NEW_GAME
               : fheroes2::GameMode::CANCEL;
}

fheroes2::GameMode Interface::Basic::EventSaveGame() const
{
    while ( true ) {
        const std::string filename = Dialog::SelectFileSave();
        if ( filename.empty() ) {
            return fheroes2::GameMode::CANCEL;
        }

        // ask overwrite?
        const Settings & conf = Settings::Get();
        if ( System::IsFile( filename ) && conf.ExtGameRewriteConfirm()
             && Dialog::NO == Dialog::Message( "", _( "Are you sure you want to overwrite the save with this name?" ), Font::BIG, Dialog::YES | Dialog::NO ) ) {
            continue;
        }

        if ( Game::Save( filename ) ) {
            Dialog::Message( "", _( "Game saved successfully." ), Font::BIG, Dialog::OK );
        }
        else {
            Dialog::Message( "", _( "There was an issue during saving." ), Font::BIG, Dialog::OK );
        }
        return fheroes2::GameMode::CANCEL;
    }
}

fheroes2::GameMode Interface::Basic::EventLoadGame() const
{
    return Dialog::YES == Dialog::Message( "", _( "Are you sure you want to load a new game? (Your current game will be lost.)" ), Font::BIG, Dialog::YES | Dialog::NO )
               ? fheroes2::GameMode::LOAD_GAME
               : fheroes2::GameMode::CANCEL;
}

void Interface::Basic::EventPuzzleMaps( void ) const
{
    world.GetKingdom( Settings::Get().CurrentColor() ).PuzzleMaps().ShowMapsDialog();
}

fheroes2::GameMode Interface::Basic::EventGameInfo()
{
    if ( Settings::Get().isCampaignGameType() ) {
        fheroes2::Display & display = fheroes2::Display::instance();
        fheroes2::ImageRestorer saver( display, 0, 0, display.width(), display.height() );

        AGG::ResetMixer();

        const fheroes2::GameMode returnMode = Game::SelectCampaignScenario( fheroes2::GameMode::CANCEL, true );
        if ( returnMode == fheroes2::GameMode::CANCEL ) {
            saver.restore();

            Game::restoreSoundsForCurrentFocus();
        }
        else {
            saver.reset();
        }

        return returnMode;
    }

    Dialog::GameInfo();
    return fheroes2::GameMode::CANCEL;
}

void Interface::Basic::EventSwitchHeroSleeping( void )
{
    Heroes * hero = GetFocusHeroes();

    if ( hero ) {
        hero->Modes( Heroes::SLEEPER ) ? hero->ResetModes( Heroes::SLEEPER ) : hero->SetModes( Heroes::SLEEPER );

        SetRedraw( REDRAW_HEROES );
        buttonsArea.SetRedraw();
    }
}

fheroes2::GameMode Interface::Basic::EventDigArtifact()
{
    Heroes * hero = GetFocusHeroes();

    if ( hero ) {
        if ( hero->isShipMaster() )
            Dialog::Message( "", _( "Try looking on land!!!" ), Font::BIG, Dialog::OK );
        else if ( hero->GetMaxMovePoints() <= hero->GetMovePoints() ) {
            if ( world.GetTiles( hero->GetIndex() ).GoodForUltimateArtifact() ) {
                AGG::PlaySound( M82::DIGSOUND );

                hero->ResetMovePoints();

                if ( world.DiggingForUltimateArtifact( hero->GetCenter() ) ) {
                    AGG::PlaySound( M82::TREASURE );
                    const Artifact & ultimate = world.GetUltimateArtifact().GetArtifact();
                    hero->PickupArtifact( ultimate );
                    std::string msg( _( "After spending many hours digging here, you have uncovered the %{artifact}." ) );
                    StringReplace( msg, "%{artifact}", ultimate.GetName() );
                    Dialog::ArtifactInfo( _( "Congratulations!" ), msg, ultimate.GetID() );
                }
                else
                    Dialog::Message( "", _( "Nothing here. Where could it be?" ), Font::BIG, Dialog::OK );

                iconsPanel.RedrawIcons( ICON_HEROES );
                fheroes2::Display::instance().render();

                // check if the game is over due to conditions related to the ultimate artifact
                return GameOver::Result::Get().LocalCheckGameOver();
            }
            else
                Dialog::Message( "", _( "Try searching on clear ground." ), Font::BIG, Dialog::OK );
        }
        else {
            Dialog::Message( "", _( "Digging for artifacts requires a whole day, try again tomorrow." ), Font::BIG, Dialog::OK );
        }
    }

    return fheroes2::GameMode::CANCEL;
}

fheroes2::GameMode Interface::Basic::EventDefaultAction( const fheroes2::GameMode gameMode )
{
    Heroes * hero = GetFocusHeroes();

    if ( hero ) {
        // 1. action object
        if ( MP2::isActionObject( hero->GetMapsObject(), hero->isShipMaster() ) ) {
            hero->Action( hero->GetIndex(), true );

            // The action object (e.g. Stables or Well) can alter the status of the hero
            iconsPanel.RedrawIcons( ICON_HEROES );

            // If a hero completed an action we must verify the condition for the scenario.
            if ( hero->isAction() ) {
                hero->ResetAction();
                // check if the game is over after the hero's action
                return GameOver::Result::Get().LocalCheckGameOver();
            }
        }
    }
    else if ( GetFocusCastle() ) {
        // 2. town dialog
        Game::OpenCastleDialog( *GetFocusCastle() );
    }

    return gameMode;
}

void Interface::Basic::EventOpenFocus( void ) const
{
    if ( GetFocusHeroes() )
        Game::OpenHeroesDialog( *GetFocusHeroes(), true, true );
    else if ( GetFocusCastle() )
        Game::OpenCastleDialog( *GetFocusCastle() );
}

void Interface::Basic::EventSwitchShowRadar( void ) const
{
    Settings & conf = Settings::Get();

    if ( conf.ExtGameHideInterface() ) {
        if ( conf.ShowRadar() ) {
            conf.SetShowRadar( false );
            gameArea.SetRedraw();
        }
        else {
            conf.SetShowRadar( true );
            radar.SetRedraw();
        }
    }
}

void Interface::Basic::EventSwitchShowButtons( void ) const
{
    Settings & conf = Settings::Get();

    if ( conf.ExtGameHideInterface() ) {
        if ( conf.ShowButtons() ) {
            conf.SetShowButtons( false );
            gameArea.SetRedraw();
        }
        else {
            conf.SetShowButtons( true );
            buttonsArea.SetRedraw();
        }
    }
}

void Interface::Basic::EventSwitchShowStatus( void ) const
{
    Settings & conf = Settings::Get();

    if ( conf.ExtGameHideInterface() ) {
        if ( conf.ShowStatus() ) {
            conf.SetShowStatus( false );
            gameArea.SetRedraw();
        }
        else {
            conf.SetShowStatus( true );
            statusWindow.SetRedraw();
        }
    }
}

void Interface::Basic::EventSwitchShowIcons( void )
{
    Settings & conf = Settings::Get();

    if ( conf.ExtGameHideInterface() ) {
        if ( conf.ShowIcons() ) {
            conf.SetShowIcons( false );
            gameArea.SetRedraw();
        }
        else {
            conf.SetShowIcons( true );
            iconsPanel.SetCurrentVisible();
            iconsPanel.SetRedraw();
        }
    }
}

void Interface::Basic::EventSwitchShowControlPanel( void ) const
{
    Settings & conf = Settings::Get();

    if ( conf.ExtGameHideInterface() ) {
        conf.SetShowPanel( !conf.ShowControlPanel() );
        gameArea.SetRedraw();
    }
}

void Interface::Basic::EventKeyArrowPress( int dir )
{
    Heroes * hero = GetFocusHeroes();

    // move hero
    if ( hero )
        MoveHeroFromArrowKeys( *hero, dir );
    else
        // scroll map
        switch ( dir ) {
        case Direction::TOP_LEFT:
            gameArea.SetScroll( SCROLL_TOP );
            gameArea.SetScroll( SCROLL_LEFT );
            break;
        case Direction::TOP:
            gameArea.SetScroll( SCROLL_TOP );
            break;
        case Direction::TOP_RIGHT:
            gameArea.SetScroll( SCROLL_TOP );
            gameArea.SetScroll( SCROLL_RIGHT );
            break;
        case Direction::RIGHT:
            gameArea.SetScroll( SCROLL_RIGHT );
            break;
        case Direction::BOTTOM_RIGHT:
            gameArea.SetScroll( SCROLL_BOTTOM );
            gameArea.SetScroll( SCROLL_RIGHT );
            break;
        case Direction::BOTTOM:
            gameArea.SetScroll( SCROLL_BOTTOM );
            break;
        case Direction::BOTTOM_LEFT:
            gameArea.SetScroll( SCROLL_BOTTOM );
            gameArea.SetScroll( SCROLL_LEFT );
            break;
        case Direction::LEFT:
            gameArea.SetScroll( SCROLL_LEFT );
            break;
        default:
            break;
        }
}
