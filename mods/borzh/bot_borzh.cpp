#include "chat.h"
#include "clients.h"
#include "planner.h"
#include "type2string.h"

#include "mods/borzh/bot_borzh.h"
#include "mods/borzh/mod_borzh.h"


#define GET_TYPE(arg)                GET_1ST_BYTE(arg)
#define SET_TYPE(type, arg)          SET_1ST_BYTE(type, arg)
#define MAKE_TYPE(type)              MAKE_1ST_BYTE(type)

#define IS_ASKED_FOR_HELP(arg)       (arg&1)
#define SET_ASKED_FOR_HELP(arg)      arg = arg | 1

#define IS_ACCEPTED(arg)             (arg&2)
#define SET_ACCEPTED(arg)            arg = arg | 2

#define IS_PUSHED(arg)               (arg&4)
#define SET_PUSHED(arg)              arg = arg | 4

#define GET_INDEX(arg)               GET_2ND_BYTE(arg)
#define SET_INDEX(index, arg)        SET_2ND_BYTE(index, arg)
#define MAKE_INDEX(index)            MAKE_2ND_BYTE(index)

#define GET_AREA(arg)                GET_2ND_BYTE(arg)
#define SET_AREA(area, arg)          SET_2ND_BYTE(area, arg)
#define MAKE_AREA(area)              MAKE_2ND_BYTE(area)

#define GET_WEAPON(arg)              GET_2ND_BYTE(arg)
#define SET_WEAPON(w, arg)           SET_2ND_BYTE(w, arg)
#define MAKE_WEAPON(w)               MAKE_2ND_BYTE(w)

#define GET_BUTTON(arg)              GET_2ND_BYTE(arg)
#define SET_BUTTON(button, arg)      SET_2ND_BYTE(button, arg)
#define MAKE_BUTTON(button)          MAKE_2ND_BYTE(button)

#define GET_DOOR(arg)                GET_3RD_BYTE(arg)
#define SET_DOOR(door, arg)          SET_3RD_BYTE(door, arg)
#define MAKE_DOOR(door)              MAKE_3RD_BYTE(door)

#define GET_DOOR_STATUS(arg)         GET_4TH_BYTE(arg)
#define SET_DOOR_STATUS(status, arg) SET_4TH_BYTE(status, arg)
#define MAKE_DOOR_STATUS(status)     MAKE_4TH_BYTE(status)

#define GET_PLAYER(arg)              GET_4TH_BYTE(arg)
#define SET_PLAYER(player, arg)      SET_4TH_BYTE(player, arg)
#define MAKE_PLAYER(player)          MAKE_4TH_BYTE(player)

#define GET_STEP(arg)                GET_4TH_BYTE(arg)
#define SET_STEP(step, arg)          SET_4TH_BYTE(step, arg)
#define MAKE_STEP(step)              MAKE_4TH_BYTE(step)


good::vector<TEntityIndex> CBotBorzh::m_aLastPushedButtons;
CBotBorzh::CBorzhTask CBotBorzh::m_cCurrentProposedTask;

//----------------------------------------------------------------------------------------------------------------
CBotBorzh::CBotBorzh( edict_t* pEdict, TPlayerIndex iIndex, TBotIntelligence iIntelligence ):
	CBot(pEdict, iIndex, iIntelligence), m_bHasCrossbow(false), m_bHasPhyscannon(false), m_bStarted(false)
{
	CBotrixPlugin::pEngineServer->SetFakeClientConVarValue(pEdict, "cl_autowepswitch", "0");	
	CBotrixPlugin::pEngineServer->SetFakeClientConVarValue(pEdict, "cl_defaultweapon", "weapon_crowbar");	
}
		
//----------------------------------------------------------------------------------------------------------------
CBotBorzh::~CBotBorzh()
{
	if ( IsPlannerTask() )
	{
		CPlanner::Stop();
		CPlanner::Unlock(this);
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::Activated()
{
	CBot::Activated();

	m_cAcceptedPlayers.resize( CPlayers::Size() );
	m_cAcceptedPlayers.reset();
	m_cBusyPlayers.resize( CPlayers::Size() );
	m_cBusyPlayers.reset();
	m_cCollaborativePlayers.resize( CPlayers::Size() );
	m_cCollaborativePlayers.reset();
	m_cCollaborativePlayers.set(m_iIndex); // Annotate self as collaborative player.
	m_cPlayersWithPhyscannon.resize( CPlayers::Size() );
	m_cPlayersWithPhyscannon.reset();
	m_cPlayersWithCrossbow.resize( CPlayers::Size() );
	m_cPlayersWithCrossbow.reset();

	m_aPlayersAreas.resize( m_cAcceptedPlayers.size() );
	memset(m_aPlayersAreas.data(), 0xFF, m_aPlayersAreas.size());

	m_aPlayersTasks.resize( m_cAcceptedPlayers.size() );
	memset(m_aPlayersTasks.data(), 0xFF, m_aPlayersTasks.size());

	// Initialize doors and buttons.
	m_cSeenDoors.resize( CItems::GetItems(EEntityTypeDoor).size() );
	m_cSeenDoors.reset();

	m_cSeenButtons.resize( CItems::GetItems(EEntityTypeButton).size() );
	m_cSeenButtons.reset();

	m_cPushedButtons.resize( CItems::GetItems(EEntityTypeButton).size() );
	m_cPushedButtons.reset();

	m_cOpenedDoors.resize( m_cSeenDoors.size() );
	m_cOpenedDoors.reset();

	m_cCheckedDoors.resize( m_cSeenButtons.size() );

	m_cFalseOpenedDoors.resize( m_cSeenDoors.size() );
	m_cFalseOpenedDoors.reset();

	m_cButtonTogglesDoor.resize( m_cSeenButtons.size()  );
	m_cButtonNoAffectDoor.resize( m_cSeenButtons.size() );
	m_cTestedToggles.resize( m_cSeenButtons.size() );
	for ( int i=0; i < m_cButtonTogglesDoor.size(); ++i )
	{
		m_cButtonTogglesDoor[i].resize( m_cSeenDoors.size() );
		m_cButtonTogglesDoor[i].reset();

		m_cButtonNoAffectDoor[i].resize( m_cSeenDoors.size() );
		m_cButtonNoAffectDoor[i].reset();

		m_cTestedToggles[i].resize( m_cSeenDoors.size() );
		m_cTestedToggles[i].reset();
	}

	m_aVisitedWaypoints.resize( CWaypoints::Size() );
	m_aVisitedWaypoints.reset();

	m_cVisitedAreas.resize( CWaypoints::GetAreas().size() );
	m_cVisitedAreas.reset();

	m_cVisitedAreasAfterPushButton.resize( m_cVisitedAreas.size() );

	m_cReachableAreas.resize( m_cVisitedAreas.size() );

	m_bSaidHello = m_bSpokenAboutAllBoxes = false;
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::Respawned()
{
	CBot::Respawned();

	m_bDontAttack = true;
	m_bNothingToDo = false;
	m_cCurrentTask.iTask = EBorzhTaskInvalid;

	if ( iCurrentWaypoint != -1 )
		m_aVisitedWaypoints.set(iCurrentWaypoint);
	m_aPlayersAreas[m_iIndex] = CWaypoints::Get(iCurrentWaypoint).iAreaId;

	m_bUsedPlannerForGoal = m_bUsedPlannerForButton = false;
	m_bWasMovingBeforePause = false;
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::HurtBy( int iPlayerIndex, CPlayer* pAttacker, int iHealthNow )
{
	m_cChat.iBotChat = EBotChatDontHurt;
	m_cChat.iDirectedTo = iPlayerIndex;
	m_cChat.cMap.clear();

	CBot::Speak(false);
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::ReceiveChat( int iPlayerIndex, CPlayer* pPlayer, bool bTeamOnly, const char* szText )
{
	good::string sText(szText, true, true);
	sText.lower_case();

	if ( !m_bStarted && (sText == "start") )
	{
		m_bNeedMove = m_bWasMovingBeforePause;
		m_bStarted = true;
	}
	else if ( m_bStarted && (sText == "pause") )
	{
		m_bWasMovingBeforePause = m_bNeedMove;
		m_bStarted = m_bNeedMove = false;
	}
	else if ( sText == "reset" )
	{
		m_bNothingToDo = false;
	}
	else if ( sText == "restart" )
	{
		m_bNothingToDo = false;
		BigTaskFinish();
		TaskFinish();
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::ReceiveChatRequest( const CBotChat& cRequest )
{
	// TODO: synchronize chat, receive in buffer at the Think().
	TChatVariableValue iVars[CModBorzh::iTotalVars];
	memset(&iVars, 0xFF, CModBorzh::iTotalVars * sizeof(TChatVariableValue));

#define AREA        iVars[CModBorzh::iVarArea]
#define BUTTON      iVars[CModBorzh::iVarButton]
#define DOOR        iVars[CModBorzh::iVarDoor]
#define DOOR_STATUS iVars[CModBorzh::iVarDoorStatus]
#define WEAPON      iVars[CModBorzh::iVarWeapon]

	// Get needed chat variables.
	for ( int i=0; i < cRequest.cMap.size(); ++i )
	{
		const CChatVarValue& cVarValue = cRequest.cMap[i];
		iVars[cVarValue.iVar] = cVarValue.iValue;
	}

	// Respond to a request/chat. TODO: check arguments.
	switch (cRequest.iBotChat)
	{
		case EBotChatGreeting:
			m_cCollaborativePlayers.set(cRequest.iSpeaker);
			break;

		case EBotChatAffirmative:
		case EBotChatAffirm:
		case EBorzhChatOk:
			if ( IsCollaborativeTask() && IS_ASKED_FOR_HELP(m_cCurrentBigTask.iArgument) && m_cCollaborativePlayers.test(cRequest.iSpeaker) )
			{
				switch( m_cCurrentBigTask.iTask )
				{
					case EBorzhTaskButtonTry:
						m_aPlayersTasks[cRequest.iSpeaker] = EBorzhTaskButtonTryHelp;
						break;
					case EBorzhTaskButtonDoorConfig:
						m_aPlayersTasks[cRequest.iSpeaker] = EBorzhTaskButtonDoorConfigHelp;
						break;
					case EBorzhTaskGoToGoal:
						m_aPlayersTasks[cRequest.iSpeaker] = EBorzhTaskGoToGoalHelp;
						break;
					default:
						DebugAssert(false);
				}
				m_cAcceptedPlayers.set(cRequest.iSpeaker);
				CheckAcceptedPlayersForCollaborativeTask();
			}
			break;

		case EBorzhChatDone:
			// TODO: remove this task, because it can be wait task when recieve chat.
			if ( IsPlannerTask() && IS_ACCEPTED(m_cCurrentBigTask.iArgument) &&
			     (m_cCurrentTask.iTask == EBorzhTaskWaitPlayer) && (m_cCurrentTask.iArgument == cRequest.iSpeaker) )
				TaskFinish();
			break;

		case EBorzhChatNoMoves:
		//case EBorzhChatFinishExplore:
			m_aPlayersTasks[cRequest.iSpeaker] = EBorzhTaskInvalid;
			m_cBusyPlayers.reset(cRequest.iSpeaker);
			if ( m_bNothingToDo && m_cBusyPlayers.none() )
				m_bNothingToDo = false;
			break;

		case EBotChatBusy:
		case EBorzhChatWait:
		case EBotChatStop:
		case EBotChatNegative:
		case EBotChatNegate:
			if ( IsCollaborativeTask() && m_cCollaborativePlayers.test(cRequest.iSpeaker) )
				UnexpectedChatForCollaborativeTask(cRequest.iSpeaker, true);
			break;

		case EBorzhChatExplore:
		case EBorzhChatExploreNew:
			m_aPlayersTasks[cRequest.iSpeaker] = EBorzhTaskExplore;
			if ( IsPlannerTask() && m_cCollaborativePlayers.test(cRequest.iSpeaker) )
				UnexpectedChatForCollaborativeTask(cRequest.iSpeaker, true);
			break;

		case EBorzhChatNewArea:
			m_cVisitedAreas.set(AREA);
			if ( IsPlannerTask() && m_cCollaborativePlayers.test(cRequest.iSpeaker) )
			{
				UnexpectedChatForCollaborativeTask(cRequest.iSpeaker, true); // Other player is investigating area, wait for him.
				break;
			}
			// Do not break.
		case EBorzhChatChangeArea:
			m_aPlayersAreas[cRequest.iSpeaker] = AREA;
			// TODO: check planner here instead of "done".
			// Check if goal of the plan is reached.
			if ( (IsPlannerTask() || IsPlannerTaskHelp()) && IsPlannerTaskFinished() )
			{
				TaskFinish();
				BigTaskFinish();
			}
			break;

		case EBorzhChatWeaponFound:
		{
			bool bIsCrossbow = (WEAPON == CModBorzh::iVarValueWeaponCrossbow);
			DebugAssert( bIsCrossbow || (WEAPON == CModBorzh::iVarValueWeaponPhyscannon) );
			if ( bIsCrossbow )
				m_cPlayersWithCrossbow.set(cRequest.iSpeaker);
			else
				m_cPlayersWithPhyscannon.set(cRequest.iSpeaker);
			break;
		}

		case EBorzhChatDoorFound:
			DebugAssert( (DOOR != EChatVariableValueInvalid) && (DOOR_STATUS != EChatVariableValueInvalid) );
			DebugAssert( !m_cSeenDoors.test(DOOR) );
			m_cSeenDoors.set(DOOR);
			m_cOpenedDoors.set(DOOR, DOOR_STATUS == CModBorzh::iVarValueDoorStatusOpened);
			if ( (rand() & 3) == 0 )
				SwitchToSpeakTask(EBorzhChatOk);
			break;

		case EBorzhChatDoorChange:
			m_bNothingToDo = false;
			DebugAssert( (DOOR != EChatVariableValueInvalid) && (DOOR_STATUS != EChatVariableValueInvalid) );
			DebugAssert( m_cSeenDoors.test(DOOR) );
			m_cOpenedDoors.set(DOOR, DOOR_STATUS == CModBorzh::iVarValueDoorStatusOpened);

			if ( IsTryingButton() && m_cCollaborativePlayers.test(cRequest.iSpeaker) && IS_PUSHED(m_cCurrentBigTask.iArgument) ) // We already pushed button.
				SetButtonTogglesDoor(m_aLastPushedButtons.back(), DOOR, true);
			if ( (rand() & 3) == 0 )
				SwitchToSpeakTask(EBorzhChatOk);
			break;

		case EBorzhChatDoorNoChange:
			DebugAssert( DOOR != EChatVariableValueInvalid );
			DebugAssert( m_cSeenDoors.test(DOOR) );
			DebugAssert( DOOR_STATUS == -1 || m_cOpenedDoors.test(DOOR) == (DOOR_STATUS == CModBorzh::iVarValueDoorStatusOpened) );
			if ( IsTryingButton() && m_cCollaborativePlayers.test(cRequest.iSpeaker) && IS_PUSHED(m_cCurrentBigTask.iArgument) ) // We already pushed button.
				SetButtonTogglesDoor(m_aLastPushedButtons.back(), DOOR, false);
			if ( (rand() & 3) == 0 )
				SwitchToSpeakTask(EBorzhChatOk);
			break;

		case EBorzhChatSeeButton:
			DebugAssert( BUTTON != EChatVariableValueInvalid );
			m_cSeenButtons.set(BUTTON);
			if ( (rand() & 3) == 0 )
				SwitchToSpeakTask(EBorzhChatOk);
			break;

		case EBorzhChatButtonCanPush:
		case EBorzhChatButtonCantPush:
		case EBorzhChatButtonWeapon:
		case EBorzhChatButtonNoWeapon:
		case EBorzhChatDoorTry:
			break;

		case EBorzhChatButtonTry:
			ReceiveTaskOffer(EBorzhTaskButtonTryHelp, BUTTON, -1, cRequest.iSpeaker);
			break;

		case EBorzhChatButtonTryGo:
		case EBorzhChatButtonDoor:
			// Other bot has plan to go to button or to go to button&door.
			//m_aPlayersTasks[cRequest.iSpeaker] = EBorzhTaskButtonDoorConfig;
			ReceiveTaskOffer(EBorzhTaskButtonDoorConfigHelp, BUTTON, DOOR, cRequest.iSpeaker);
			break;

		case EBorzhChatButtonToggles:
		case EBorzhChatButtonNoToggles:
			//m_bNothingToDo = false;
			if ( cRequest.iBotChat == EBorzhChatButtonToggles )
				m_cButtonTogglesDoor[BUTTON].set(DOOR);
			else
				m_cButtonNoAffectDoor[BUTTON].set(DOOR);

			if ( IsTryingButton() && (GET_BUTTON(m_cCurrentBigTask.iArgument) == BUTTON) && (GET_DOOR(m_cCurrentBigTask.iArgument) == DOOR) )
			{
				CancelTasksInStack();
				TaskFinish();
			}

			if ( IsPlannerTask() && m_cCollaborativePlayers.test(cRequest.iSpeaker) )
				UnexpectedChatForCollaborativeTask(cRequest.iSpeaker, false);
			break;

		case EBorzhChatButtonIPush:
		case EBorzhChatButtonIShoot:
			ButtonPushed(BUTTON);

			// TODO: check planner here instead of "done".
			if ( IsPlannerTask() && m_cCollaborativePlayers.test(cRequest.iSpeaker) )
			{
				if ( GetPlanStepPerformer() == cRequest.iSpeaker )
				{
					// TODO: control that needed button is pressed.
					//PlanStepControl(iSpeaker, BUTTON);
				}
				else
					CancelCollaborativeTask();
			}
			else if ( (m_cCurrentBigTask.iTask == EBorzhTaskButtonTryHelp) && (GET_BUTTON(m_cCurrentBigTask.iArgument) == BUTTON) )
			{
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWait, m_iTimeAfterSpeak) ); // Wait time after speak, so another bot can press button.
			}
			else
			{
				if ( m_cCurrentBigTask.iTask == EBorzhTaskInvalid )
				{
					// Start new task: try button.
					m_cCurrentBigTask.iTask = EBorzhTaskButtonTryHelp;
					m_cCurrentBigTask.iArgument = 0;
					SET_BUTTON(BUTTON, m_cCurrentBigTask.iArgument);
					SET_DOOR(0xFF, m_cCurrentBigTask.iArgument);
					m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWait, m_iTimeAfterSpeak) ); // Wait time after speak, so another bot can press button.
					m_bNothingToDo = false; // Wake up bot, if it has nothing to do.
				}
				//else if ( (m_cCurrentBigTask.iTask != EBorzhTaskButtonDoorConfigHelp) && (m_cCurrentBigTask.iTask != EBorzhTaskButtonDoorConfigHelp) )
				//	SwitchToSpeakTask(EBorzhChatWait); // Tell player to wait, so bot can finish his business first.
			}
			break;

		case EBorzhChatButtonYouPush:
		case EBorzhChatButtonYouShoot:
			DebugAssert( cRequest.iDirectedTo == m_iIndex );

			if ( IsPlannerTaskHelp() && (cRequest.iSpeaker == GET_PLAYER(m_cCurrentBigTask.iArgument)) )
			{
				PushSpeakTask(EBorzhChatDone, MAKE_PLAYER(cRequest.iSpeaker));
				PushPressButtonTask(BUTTON, cRequest.iBotChat == EBorzhChatButtonYouShoot);
			}
			else
				SwitchToSpeakTask(EBotChatBusy, MAKE_PLAYER(cRequest.iSpeaker));
			break;

		case EBorzhChatAreaGo:
			DebugAssert( cRequest.iDirectedTo == m_iIndex );

			if ( IsPlannerTaskHelp() && (cRequest.iSpeaker == GET_PLAYER(m_cCurrentBigTask.iArgument)) )
			{
				PushSpeakTask(EBorzhChatDone, MAKE_PLAYER(cRequest.iSpeaker));
				TWaypointId iWaypoint = CModBorzh::GetRandomAreaWaypoint(AREA);
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskMove, iWaypoint) );
			}
			else
				SwitchToSpeakTask(EBotChatBusy, MAKE_PLAYER(cRequest.iSpeaker));
			break;

		case EBorzhChatDoorGo:
			DebugAssert( cRequest.iDirectedTo == m_iIndex );

			if ( IsPlannerTaskHelp() && (cRequest.iSpeaker == GET_PLAYER(m_cCurrentBigTask.iArgument)) )
			{
				TWaypointId iWaypoint = GetDoorWaypoint(DOOR, m_cReachableAreas);
				if ( iWaypoint == EWaypointIdInvalid )
				{
					// TODO: say that can't pass to door waypoint.
					SwitchToSpeakTask(EBotChatBusy, MAKE_PLAYER(cRequest.iSpeaker));
				}
				else
				{
					PushSpeakTask(EBorzhChatDone, MAKE_PLAYER(cRequest.iSpeaker));
					m_cTaskStack.push_back( CBorzhTask(EBorzhTaskMove, iWaypoint) );
					//SwitchToSpeakTask(EBorzhChatOk, MAKE_PLAYER(cRequest.iSpeaker));
				}
			}
			else
				SwitchToSpeakTask(EBotChatBusy, MAKE_PLAYER(cRequest.iSpeaker));
			break;

		case EBorzhChatButtonGo:
			DebugAssert(false);
			break;

		case EBorzhChatAreaCantGo:
			if ( IsPlannerTask() && m_cCollaborativePlayers.test(cRequest.iSpeaker) )
				CancelCollaborativeTask();
			break;

		case EBorzhChatTaskCancel:
			if ( IsPlannerTaskHelp() && m_cCollaborativePlayers.test(cRequest.iSpeaker) )
				CancelCollaborativeTask();
			break;

		case EBorzhChatFoundPlan:
			break;
	}

	// Wait some time, emulating processing of message.
	Wait(m_iTimeAfterSpeak, true);
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::Think()
{
	if ( !m_bAlive )
	{
		m_cCmd.buttons = rand() & IN_ATTACK; // Force bot to respawn by hitting randomly attack button.
		return;
	}

	if ( !m_bStarted || ( m_bNothingToDo && (m_cCurrentTask.iTask == EBorzhTaskInvalid) && m_cTaskStack.empty() ) )
		return;

	if ( !m_bSaidHello )
	{
		m_bSaidHello = true;
		Start();
	}

	// Check if there are some new tasks.
	if ( m_bTaskFinished || (m_cCurrentTask.iTask == EBorzhTaskInvalid) )
		TaskPop();

	if ( !m_bNewTask && (m_cCurrentTask.iTask == EBorzhTaskInvalid) )
	{
		if ( !CheckBigTask() )
		{
			CheckForNewTasks();
			if ( m_cCurrentBigTask.iTask != EBorzhTaskInvalid )
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWait, 3000) ); // Wait 3 seconds before changing task.
		}
	}

	if ( m_bNewTask || m_bMoveFailure || m_bStuck ) // Try again to go to waypoint in case of move failure.
		InitNewTask();

	DebugAssert( !m_bNewTask );

	// Update current task.
	if ( !m_bTaskFinished )
	{
		switch (m_cCurrentTask.iTask)
		{
			case EBorzhTaskWait:
				if ( CBotrixPlugin::fTime >= m_fEndWaitTime ) // Bot finished waiting.
					TaskFinish();
				break;

			case EBorzhTaskLook:
				if ( !m_bNeedAim ) // Bot finished aiming.
					TaskFinish();
				break;

			case EBorzhTaskMove:
				if ( m_bNeedMove )
				{
					if ( !m_bSpokenAboutAllBoxes )
					{
						// TODO: Check boxes.
					}
				}
				else // Bot finished moving.
					TaskFinish();
				break;

			case EBorzhTaskSpeak:
				TaskFinish();
				break;

			case EBorzhTaskWaitButton:
				DebugAssert( m_cCurrentBigTask.iTask == EBorzhTaskButtonTryHelp );
				if ( IS_PUSHED(m_cCurrentBigTask.iArgument) ) // We already pushed button.
				{
					TaskFinish();
					// TODO: EBorzhTaskCheckDoor -> DoorsCheckAtCurrentWaypoint();
				}
				break;

			case EBorzhTaskWaitPlanner:
				if ( !CPlanner::IsRunning() )
				{
					TaskFinish();
					const CPlanner::CPlan* pPlan = CPlanner::GetPlan();
					// Check if has plan, but discard empty plans for new buttons.
					if ( pPlan == NULL )
					{
						BotMessage("%s -> planner finished without plan.", GetName());
					}
					else if ( pPlan->size() == 0 )
					{
						BotMessage("%s -> planner finished with empty plan, wait for another bot to propose it.", GetName());
						BigTaskFinish();
						m_bUsedPlannerForButton = true;
					}
					else
					{
						BotMessage("%s -> planner finished with a non empty plan.", GetName());
						OfferCollaborativeTask();
					}
				}
				break;
		}
	}
}


//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::CurrentWaypointJustChanged()
{
	CBot::CurrentWaypointJustChanged();
	// Don't do processing of paths if bot is stucked.
	if ( m_bRepeatWaypointAction )
		return;
	
	m_aVisitedWaypoints.set(iCurrentWaypoint);
	const CWaypoint& cWaypoint = CWaypoints::Get(iCurrentWaypoint);

	DoorsCheckAtCurrentWaypoint();

	// Check if bot saw the button before.
	if ( FLAG_SOME_SET(FWaypointButton, cWaypoint.iFlags) )
	{
		TEntityIndex iButton = CWaypoint::GetButton(cWaypoint.iArgument);
		if (iButton > 0)
		{
			--iButton;
			if ( !m_cSeenButtons.test(iButton) ) // Bot sees button for the first time.
			{
				m_cSeenButtons.set(iButton);
				SwitchToSpeakTask( EBorzhChatSeeButton, MAKE_BUTTON(iButton), EEntityTypeButton, iButton );
			}
		}
		else
			BotMessage("%s -> Error, waypoint %d has invalid button index.", GetName(), iCurrentWaypoint);
	}
}

//----------------------------------------------------------------------------------------------------------------
bool CBotBorzh::DoWaypointAction()
{
	if ( m_bRepeatWaypointAction )
		return CBot::DoWaypointAction();

	const CWaypoint& cWaypoint = CWaypoints::Get(iCurrentWaypoint);

	// Check if bot enters new area.
	if ( cWaypoint.iAreaId != m_aPlayersAreas[m_iIndex] )
	{
		m_aPlayersAreas[m_iIndex] = cWaypoint.iAreaId;

		// Check if goal of the plan is reached.
		if ( (IsPlannerTask() || IsPlannerTaskHelp()) && IsPlannerTaskFinished() )
		{
			TaskFinish();
			BigTaskFinish();
		}

		if ( m_cVisitedAreas.test(m_aPlayersAreas[m_iIndex]) )
			SwitchToSpeakTask(EBorzhChatChangeArea, MAKE_AREA(m_aPlayersAreas[m_iIndex]));
		else
			SwitchToSpeakTask(EBorzhChatNewArea, MAKE_AREA(m_aPlayersAreas[m_iIndex]));

		m_cVisitedAreas.set(m_aPlayersAreas[m_iIndex]);
		m_cVisitedAreasAfterPushButton.set(m_aPlayersAreas[m_iIndex]);
	}

	// Check if bot saw the button to shoot before.
	if ( FLAG_SOME_SET(FWaypointSeeButton, cWaypoint.iFlags) )
	{
		TEntityIndex iButton = CWaypoint::GetButton(cWaypoint.iArgument);
		if (iButton > 0)
		{
			--iButton;
			if ( !m_cSeenButtons.test(iButton) ) // Bot sees button for the first time.
			{
				SwitchToSpeakTask( m_cPlayersWithCrossbow.test(m_iIndex) ? EBorzhChatButtonWeapon : EBorzhChatButtonNoWeapon );
				SwitchToSpeakTask( EBorzhChatSeeButton, MAKE_BUTTON(iButton), EEntityTypeButton, iButton );
				m_cSeenButtons.set(iButton);
			}
		}
		else
			BotMessage("%s -> Error, waypoint %d has invalid button index.", GetName(), iCurrentWaypoint);
	}

	return CBot::DoWaypointAction();
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::ApplyPathFlags()
{
	CBot::ApplyPathFlags();
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::DoPathAction()
{
	CBot::DoPathAction();
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::PickItem( const CEntity& cItem, TEntityType iEntityType, TEntityIndex iIndex )
{
	CBot::PickItem( cItem, iEntityType, iIndex );
	if ( iEntityType == EEntityTypeWeapon )
	{
		TEntityIndex iIdx;
		if ( cItem.pItemClass->sClassName == "weapon_crossbow" ) 
		{
			m_cPlayersWithCrossbow.set(m_iIndex);
			iIdx = CModBorzh::iVarValueWeaponCrossbow;
		}
		else if ( cItem.pItemClass->sClassName == "weapon_physcannon" ) 
		{
			m_cPlayersWithPhyscannon.set(m_iIndex);
			iIdx = CModBorzh::iVarValueWeaponPhyscannon;
		}
		else
			DebugAssert(false);
		SwitchToSpeakTask(EBorzhChatWeaponFound, MAKE_WEAPON(iIdx));
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::SetReachableAreas( int iCurrentArea, const good::bitset& cSeenDoors, const good::bitset& cOpenedDoors, good::bitset& cReachableAreas )
{
	cReachableAreas.reset();

	good::vector<TAreaId> cToVisit( CWaypoints::GetAreas().size() );
	cToVisit.push_back( iCurrentArea );

	const good::vector<CEntity>& cDoorEntities = CItems::GetItems(EEntityTypeDoor);
	while ( !cToVisit.empty() )
	{
		int iArea = cToVisit.back();
		cToVisit.pop_back();

		cReachableAreas.set(iArea);

		const good::vector<TEntityIndex>& cDoors = CModBorzh::GetDoorsForArea(iArea);
		for ( TEntityIndex i = 0; i < cDoors.size(); ++i )
		{
			const CEntity& cDoor = cDoorEntities[ cDoors[i] ];
			if ( cSeenDoors.test( cDoors[i] ) && cOpenedDoors.test( cDoors[i] ) ) // Seen and opened door.
			{
				TWaypointId iWaypoint1 = cDoor.iWaypoint;
				TWaypointId iWaypoint2 = (TWaypointId)cDoor.pArguments;
				if ( CWaypoints::IsValid(iWaypoint1) && CWaypoints::IsValid(iWaypoint2) )
				{
					TAreaId iArea1 = CWaypoints::Get(iWaypoint1).iAreaId;
					TAreaId iArea2 = CWaypoints::Get(iWaypoint2).iAreaId;
					TAreaId iNewArea = ( iArea1 == iArea ) ? iArea2 : iArea1;
					if ( !cReachableAreas.test(iNewArea) )
						cToVisit.push_back(iNewArea);
				}
				else
					DebugAssert(false);
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------
TWaypointId CBotBorzh::GetButtonWaypoint( TEntityIndex iButton, const good::bitset& cReachableAreas )
{
	const good::vector<CEntity>& cButtonEntities = CItems::GetItems(EEntityTypeButton);
	const CEntity& cButton = cButtonEntities[ iButton ];

	if ( CWaypoints::IsValid(cButton.iWaypoint) )
		return cReachableAreas.test( CWaypoints::Get(cButton.iWaypoint).iAreaId ) ? cButton.iWaypoint : EWaypointIdInvalid;
	return false;
}

//----------------------------------------------------------------------------------------------------------------
TWaypointId CBotBorzh::GetDoorWaypoint( TEntityIndex iDoor, const good::bitset& cReachableAreas )
{
	const good::vector<CEntity>& cDoorEntities = CItems::GetItems(EEntityTypeDoor);
	const CEntity& cDoor = cDoorEntities[ iDoor ];

	TWaypointId iWaypoint1 = cDoor.iWaypoint;
	TWaypointId iWaypoint2 = (TWaypointId)cDoor.pArguments;
	if ( CWaypoints::IsValid(iWaypoint1) && CWaypoints::IsValid(iWaypoint2) )
	{
		TAreaId iArea1 = CWaypoints::Get(iWaypoint1).iAreaId;
		TAreaId iArea2 = CWaypoints::Get(iWaypoint2).iAreaId;
		return cReachableAreas.test(iArea1) ? iWaypoint1 : (cReachableAreas.test(iArea2) ? iWaypoint2 : EWaypointIdInvalid);
	}
	else
		DebugAssert(false);
	return false;
}

//----------------------------------------------------------------------------------------------------------------
TPlayerIndex CBotBorzh::GetPlanStepPerformer()
{
	DebugAssert( IsPlannerTask() );
	const CPlanner::CPlan* pPlan = CPlanner::GetPlan();
	DebugAssert( pPlan );

	int iLastStep = GET_STEP(m_cCurrentBigTask.iArgument) - 1;

	if ( 0 <= iLastStep && iLastStep < pPlan->size() )
		return pPlan->at(iLastStep).iExecutioner;
	else
		return EPlayerIndexInvalid;
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::DoorsCheckAtCurrentWaypoint()
{
	// Check neighbours of a new waypoint.
	const CWaypoints::WaypointNode& cNode = CWaypoints::GetNode(iCurrentWaypoint);
	const CWaypoints::WaypointNode::arcs_t& cNeighbours = cNode.neighbours;
	for ( int i=0; i < cNeighbours.size(); ++i)
	{
		const CWaypoints::WaypointArc& cArc = cNeighbours[i];
		TWaypointId iWaypoint = cArc.target;
		if ( iWaypoint != iPrevWaypoint ) // Bot is not coming from there.
		{
			const CWaypointPath& cPath = cArc.edge;
			if ( FLAG_SOME_SET(FPathDoor, cPath.iFlags) ) // Waypoint path is passing through a door.
			{
				TEntityIndex iDoor = cPath.iArgument;
				if ( iDoor > 0 )
				{
					--iDoor;

					bool bOpened = CItems::IsDoorOpened(iDoor);
					bool bNeedToPassThrough = (iWaypoint == m_iAfterNextWaypoint) && m_bNeedMove && m_bUseNavigatorToMove;
					DoorStatusCheck(iDoor, bOpened, bNeedToPassThrough);
				}
				else
					BotMessage("%s -> Error, waypoint path from %d to %d has invalid door index.", GetName(), iCurrentWaypoint, iWaypoint);
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::DoorStatusCheck( TEntityIndex iDoor, bool bOpened, bool bNeedToPassThrough )
{
	bool bCheckingDoors = IsTryingButton() && IS_PUSHED(m_cCurrentBigTask.iArgument); // We already pushed button.

	if ( !m_cSeenDoors.test(iDoor) ) // Bot sees door for the first time.
	{
		DebugAssert( m_cCurrentBigTask.iTask == EBorzhTaskExplore ); // Should occur only when exploring new area.
		m_cSeenDoors.set(iDoor);
		m_cOpenedDoors.set(iDoor, bOpened);

		TChatVariableValue iDoorStatus = bOpened ? CModBorzh::iVarValueDoorStatusOpened : CModBorzh::iVarValueDoorStatusClosed;
		SwitchToSpeakTask(EBorzhChatDoorFound, MAKE_DOOR(iDoor) | MAKE_DOOR_STATUS(iDoorStatus), EEntityTypeDoor, iDoor );

		if ( bOpened )
			SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);
	}
	else if ( !bOpened && bNeedToPassThrough ) // Bot needs to pass through the door, but door is closed.
		DoorClosedOnTheWay(iDoor, bCheckingDoors);
	else if ( m_cOpenedDoors.test(iDoor) != bOpened ) // Bot belief of opened/closed door is different.
		DoorStatusDifferent(iDoor, bOpened, bCheckingDoors);
	else 
		DoorStatusSame(iDoor, bOpened, bCheckingDoors);
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::DoorStatusSame( TEntityIndex iDoor, bool bOpened, bool bCheckingDoors )
{
	if ( bCheckingDoors && !m_cCheckedDoors.test(iDoor) ) // Checking doors but this one is not checked.
	{
		m_cCheckedDoors.set(iDoor);

		// Button that we are checking doesn't affect iDoor.
		TEntityIndex iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);
		SetButtonTogglesDoor(iButton, iDoor, false);
		SwitchToSpeakTask(EBorzhChatButtonNoToggles, MAKE_BUTTON(iButton) | MAKE_DOOR(iDoor));

		// Say: door $door has not changed.
		TChatVariableValue iDoorStatus = bOpened ? CModBorzh::iVarValueDoorStatusOpened : CModBorzh::iVarValueDoorStatusClosed;
		SwitchToSpeakTask(EBorzhChatDoorNoChange, MAKE_DOOR(iDoor) | MAKE_DOOR_STATUS(iDoorStatus), EEntityTypeDoor, iDoor );
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::DoorStatusDifferent( TEntityIndex iDoor, bool bOpened, bool bCheckingDoors )
{
	m_cOpenedDoors.set(iDoor, bOpened);
	if ( bCheckingDoors )
	{
		DebugAssert( !m_cCheckedDoors.test(iDoor) ); // Door should not be checked already.
		m_cCheckedDoors.set(iDoor);

		// Button that we are checking opens iDoor.
		TEntityIndex iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);
		SetButtonTogglesDoor(iButton, iDoor, true);
		SwitchToSpeakTask(EBorzhChatButtonToggles, MAKE_BUTTON(iButton) | MAKE_DOOR(iDoor));

		// If door is opened, don't check areas behind it, for now. Will check after finishing checking doors.
		if ( !bOpened )
		{
			// Bot was thinking that this door was opened. Recalculate reachable areas (closing this door).
			m_cFalseOpenedDoors.reset(iDoor);

			// Recalculate reachable areas using false opened doors.
			SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cFalseOpenedDoors, m_cReachableAreas);
		}
	}
	// Say: door $door is now $door_status.
	TChatVariableValue iDoorStatus = bOpened ? CModBorzh::iVarValueDoorStatusOpened : CModBorzh::iVarValueDoorStatusClosed;
	SwitchToSpeakTask(EBorzhChatDoorChange, MAKE_DOOR(iDoor) | MAKE_DOOR_STATUS(iDoorStatus), EEntityTypeDoor, iDoor);
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::DoorClosedOnTheWay( TEntityIndex iDoor, bool bCheckingDoors )
{
	DebugAssert( m_cOpenedDoors.test(iDoor) ); // Bot should think that this door was opened.
	m_cOpenedDoors.reset(iDoor); // Close the door.

	CancelTasksInStack();
	TaskFinish();
	if ( bCheckingDoors )
	{
		m_cCheckedDoors.set(iDoor);

		TEntityIndex iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);
		SetButtonTogglesDoor(iButton, iDoor, true);
		PushSpeakTask(EBorzhChatButtonToggles, MAKE_BUTTON(iButton) | MAKE_DOOR(iDoor));

		m_cFalseOpenedDoors.reset(iDoor); // Close the door.
		// Recalculate reachable areas using false opened doors.
		SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cFalseOpenedDoors, m_cReachableAreas);
	}
	else
	{
		// Say: I can't reach area $area.
		TAreaId iArea = CWaypoints::Get( m_iDestinationWaypoint ).iAreaId;
		PushSpeakTask(EBorzhChatAreaCantGo, MAKE_AREA(iArea));

		// Recalculate reachable areas using opened doors.
		SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);
	}

	// Say: Door $door is closed now.
	PushSpeakTask(EBorzhChatDoorChange, MAKE_DOOR(iDoor) | MAKE_DOOR_STATUS(CModBorzh::iVarValueDoorStatusClosed), EEntityTypeDoor, iDoor);
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::Start()
{
	SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);

	if ( !m_cVisitedAreas.test(m_aPlayersAreas[m_iIndex]) )
		PushSpeakTask(EBorzhChatNewArea, MAKE_AREA(m_aPlayersAreas[m_iIndex])); // Say: i am in new area.
	else
		PushSpeakTask(EBorzhChatChangeArea, MAKE_AREA(m_aPlayersAreas[m_iIndex])); // Say: i have changed area.

	// Say hello to some other player.
	for ( TPlayerIndex iPlayer = 0; iPlayer < CPlayers::Size(); ++iPlayer )
	{
		CPlayer* pPlayer = CPlayers::Get(iPlayer);
		if ( pPlayer && (m_iIndex != iPlayer) && (pPlayer->GetTeam() == GetTeam()) )
			PushSpeakTask(EBotChatGreeting, MAKE_PLAYER(iPlayer));
	}
	TaskFinish();
}

//----------------------------------------------------------------------------------------------------------------
bool CBotBorzh::CheckBigTask()
{
	bool bHasTask = false;
	DebugAssert( m_cCurrentTask.iTask == EBorzhTaskInvalid );
	switch ( m_cCurrentBigTask.iTask )
	{
		case EBorzhTaskExplore:
		{
			TAreaId iArea = m_cCurrentBigTask.iArgument;

			const good::vector<TWaypointId>& cWaypoints = CModBorzh::GetWaypointsForArea(iArea);
			DebugAssert( cWaypoints.size() > 0 );

			TWaypointId iWaypoint = EWaypointIdInvalid;
			int iIndex = rand() % cWaypoints.size();

			// Search for some not visited waypoint in this area.
			for ( int i=iIndex; i >= 0; --i)
			{
				int iCurrent = cWaypoints[i];
				if ( (iCurrent != iCurrentWaypoint) && !m_aVisitedWaypoints.test(iCurrent) )
				{
					iWaypoint = iCurrent;
					break;
				}
			}
			if ( iWaypoint == EWaypointIdInvalid )
			{
				for ( int i=iIndex+1; i < cWaypoints.size(); ++i)
				{
					int iCurrent = cWaypoints[i];
					if ( (iCurrent != iCurrentWaypoint) && !m_aVisitedWaypoints.test(iCurrent) )
					{
						iWaypoint = iCurrent;
						break;
					}
				}
			}

			DebugAssert( iWaypoint != iCurrentWaypoint );
			if ( iWaypoint == EWaypointIdInvalid )
			{
				m_cVisitedAreas.set(iArea);
				BigTaskFinish();
				PushSpeakTask(EBorzhChatFinishExplore, MAKE_AREA(m_aPlayersAreas[m_iIndex]));
			}
			else // Start task to move to given waypoint.
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskMove, iWaypoint) );
			bHasTask = true;
			break;
		}

		case EBorzhTaskButtonTry:
		case EBorzhTaskButtonTryHelp:
		{
			TEntityIndex iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);
			const good::bitset& cButtonTogglesDoor = m_cButtonTogglesDoor[iButton];
			const good::bitset& cButtonNoAffectDoor = m_cButtonNoAffectDoor[iButton];

			//SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);

			const good::vector<CEntity>& aDoors = CItems::GetItems(EEntityTypeDoor);
			for ( int iDoor = 0; iDoor < aDoors.size(); ++iDoor )
				if ( !m_cCheckedDoors.test(iDoor) && !cButtonTogglesDoor.test(iDoor) && !cButtonNoAffectDoor.test(iDoor) )
				{
					TWaypointId iWaypoint = GetDoorWaypoint(iDoor, m_cReachableAreas);
					if ( iWaypoint != EWaypointIdInvalid )
					{
						m_cTaskStack.push_back( CBorzhTask(EBorzhTaskMove, iWaypoint) );
						SET_DOOR(iDoor, m_cCurrentBigTask.iArgument); // Going to that door.
						return true;
					}
				}

			// Recalculate reachable areas.
			SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);

			BigTaskFinish();
			bHasTask = false;
			break;
		}

		case EBorzhTaskButtonDoorConfig:
			if ( IS_ACCEPTED(m_cCurrentBigTask.iArgument) ) // Already executing plan.
				bHasTask = PlanStepNext();
			else
				bHasTask = CheckButtonDoorConfigurations(); // Plan failed, check other button-door configuration.
			break;

		case EBorzhTaskButtonDoorConfigHelp:
			bHasTask = true; // Wait for player to command.
			break;
	}
	return bHasTask;
}

//----------------------------------------------------------------------------------------------------------------
bool CBotBorzh::CheckForNewTasks( TBorzhTask iProposedTask )
{
	// Investigate buttons task are very important, only they can determine if button toggles a door, so don't accept any other task.
	if ( iProposedTask != EBorzhTaskInvalid && IsTryingButton() && IS_ASKED_FOR_HELP(m_cCurrentBigTask.iArgument) )
		return false; // Don't accept any other task.

	// Check if all bots can pass to goal area. Bot should have been at least in goal area.
	if ( iProposedTask >= EBorzhTaskGoToGoal )
		return true;

	int iGoalArea = CWaypoints::GetAreas().size() - 1;
	if ( (iProposedTask == EBorzhTaskInvalid) && m_cVisitedAreas.test(iGoalArea) && !m_bUsedPlannerForGoal && !CPlanner::IsLocked() )
	{
		StartPlannerForGoal();
		return false;
	}

	// Check if there is some area to investigate.
	if ( iProposedTask >= EBorzhTaskExplore )
		return true;

	const StringVector& aAreas = CWaypoints::GetAreas();
	for ( TAreaId iArea = 0; iArea < aAreas.size(); ++iArea )
	{
		if ( !m_cVisitedAreas.test(iArea) && m_cReachableAreas.test(iArea) )
		{
			BotMessage( "%s -> CheckForNewTasks(): explore new area %s.", GetName(), CWaypoints::GetAreas()[iArea].c_str() );
			m_cCurrentBigTask.iTask = EBorzhTaskExplore;
			m_cCurrentBigTask.iArgument = iArea;
			SwitchToSpeakTask(EBorzhChatExploreNew, MAKE_AREA(m_aPlayersAreas[m_iIndex]));
			return false;
		}
	}

	// Check if there are some button to push.
	if ( iProposedTask >= EBorzhTaskButtonTry )
		return true;

	const good::vector<CEntity>& aButtons = CItems::GetItems(EEntityTypeButton);
	const good::vector<CEntity>& aDoors = CItems::GetItems(EEntityTypeDoor);

	for ( TEntityIndex iButton = 0; iButton < aButtons.size(); ++iButton )
	{
		if ( m_cSeenButtons.test(iButton) && (GetButtonWaypoint(iButton, m_cReachableAreas) != EWaypointIdInvalid) ) // Can reach button.
		{
			TEntityIndex iDoor = EEntityIndexInvalid;

			int iToggleCount = m_cButtonTogglesDoor[iButton].count();
			int iNoToggleCount = m_cButtonNoAffectDoor[iButton].count();

			// Try button if it is not pushed.
			bool bShoot = (aButtons[iButton].iWaypoint == EWaypointIdInvalid);
			if (bShoot && !m_cPlayersWithCrossbow.test(m_iIndex)) // TODO: check bullets.
				continue;

			bool bTryButton = !m_cPushedButtons.test(iButton);

			// Try button if it opens less than 2 doors it is and there is a door it is not known to toggle.
			if ( !bTryButton && (iToggleCount < 2) && (iToggleCount+iNoToggleCount < aDoors.size() ))
			{
				/*
				m_cFalseOpenedDoors.reset();
				m_cFalseOpenedDoors |= m_cOpenedDoors; // Copy opened door to aux.

				// Emulate that button is pushed so some doors change state.
				for ( iDoor = 0; iDoor < aDoors.size(); ++iDoor )
					if ( m_cButtonTogglesDoor[iButton].test(iDoor) )
						m_cFalseOpenedDoors.set( iDoor, !m_cFalseOpenedDoors.test(iDoor) );

				m_cAuxReachableAreas.assign( m_cReachableAreas, true, true ); // Copy reachable areas to aux.
				SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cFalseOpenedDoors, m_cAuxReachableAreas);
				*/

				for ( iDoor = 0; iDoor < aDoors.size(); ++iDoor )
				{
					if ( !m_cButtonTogglesDoor[iButton].test(iDoor) && !m_cButtonNoAffectDoor[iButton].test(iDoor) )
					{
						const CEntity& cDoor = aDoors[iDoor];
						for ( TPlayerIndex iPlayer = 0; iPlayer < CPlayers::Size(); ++iPlayer )
						{
							if ( (iPlayer != m_iIndex) && m_cCollaborativePlayers.test(iPlayer) && IsPlayerInDoorArea(iPlayer, cDoor) )
							{
								bTryButton = true;
								break;
							}
						}
						if ( bTryButton )
							break;
					}
				}
			}
			if ( bTryButton )
			{
				// We have at least one player to investigate door next to him.
				BotMessage( "%s -> CheckForNewTasks(): investigate button%d (possible to check door%d).", GetName(), iButton+1, iDoor+1 );
				// TODO: check door and use it in speak task.
				PushCheckButtonTask(iButton, bShoot);
				return false;
			}
		}
	}

	// Check if there are unknown button-door configuration to check (without pushing intermediate buttons).
	if ( iProposedTask >= EBorzhTaskButtonDoorConfig )
		return true;

	if ( (iProposedTask == EBorzhTaskInvalid) && !m_bUsedPlannerForButton && !CPlanner::IsLocked() )
	{
		m_cCurrentBigTask.iArgument = 0; // Start with button 0 and door 0.
		CPlanner::Lock(this);
		if ( CheckButtonDoorConfigurations() )
			return false; // Don't accept proposed task, use EBorzhTaskButtonDoorConfig.
		CPlanner::Unlock(this);
	}

	// Nothing to do.
	if ( iProposedTask == EBorzhTaskInvalid )
	{
		m_bUsedPlannerForGoal = false;
		m_bUsedPlannerForButton = false;

		// Bot has nothing to do, wait for domain change.
		PushSpeakTask(EBorzhChatNoMoves);
		m_bNothingToDo = true;
		return false;
	}
	else
		return true; // Accept any task, we have nothing to do.
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::InitNewTask()
{
	//BotMessage( "%s -> New task %s.", GetName(), CTypeToString::BorzhTaskToString(m_cCurrentTask.iTask).c_str() );

	m_bNewTask = m_bTaskFinished = false;

	switch ( m_cCurrentTask.iTask )
	{
		case EBorzhTaskWaitAnswer:
			CheckAcceptedPlayersForCollaborativeTask();
		case EBorzhTaskWait:
			m_fEndWaitTime = CBotrixPlugin::fTime + m_cCurrentTask.iArgument/1000.0f;
			break;

		case EBorzhTaskLook:
		{
			TEntityType iType = GET_TYPE(m_cCurrentTask.iArgument);
			TEntityIndex iIndex = GET_INDEX(m_cCurrentTask.iArgument);
			const CEntity& cEntity = CItems::GetItems(iType)[iIndex];
			m_vLook = cEntity.vOrigin;
			m_bNeedMove = false;
			m_bNeedAim = true;
			break;
		}

		case EBorzhTaskMove:
			if ( iCurrentWaypoint == m_cCurrentTask.iArgument )
			{
				m_bNeedMove = m_bUseNavigatorToMove = m_bDestinationChanged = false;
				TaskFinish();
				m_bRepeatWaypointAction = false;
				CurrentWaypointJustChanged();
			}
			else
			{
				m_bNeedMove = m_bUseNavigatorToMove = true;
				if ( m_bMoveFailure || m_bStuck || (m_iDestinationWaypoint != m_cCurrentTask.iArgument) )
				{
					m_bMoveFailure = m_bStuck = false;
					m_iDestinationWaypoint = m_cCurrentTask.iArgument;
					m_bDestinationChanged = true;
				}
			}
			break;

		case EBorzhTaskSpeak:
			m_bNeedMove = false;
			DoSpeakTask(m_cCurrentTask.iArgument);
			break;

		case EBorzhTaskPushButton:
			FLAG_SET(IN_USE, m_cCmd.buttons);
			m_aLastPushedButtons.push_back(m_cCurrentTask.iArgument);
			ButtonPushed(m_cCurrentTask.iArgument);
			Wait(m_iTimeAfterPushingButton); // Wait 2 seconds after pushing button.
			break;

		case EBorzhTaskWeaponSet:
			DebugAssert ( m_iCrossbow >= 0 );
			if ( m_iWeapon != m_iCrossbow )
			{
				SetActiveWeapon( m_iCrossbow );
				Wait( m_aWeapons[m_iCrossbow].GetBaseWeapon()->fHolsterTime + 0.5f ); // Wait for holster + half second to make sure.
			}
			else
				TaskFinish();
			break;

		case EBorzhTaskWeaponZoom:
		case EBorzhTaskWeaponRemoveZoom:
			DebugAssert( m_aWeapons[m_iCrossbow].IsUsingZoom() == (m_cCurrentTask.iTask == EBorzhTaskWeaponRemoveZoom) );
			ToggleZoom();
			Wait( m_aWeapons[m_iCrossbow].GetBaseWeapon()->fShotTime[1] + 0.5f ); // Wait for zoom + half second to make sure.
			break;

		case EBorzhTaskWeaponShoot:
			Shoot();
			Wait( m_aWeapons[m_iCrossbow].GetBaseWeapon()->fShotTime[0] + 0.5f ); // Wait for shoot + half second to make sure.
			break;
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::PushSpeakTask( TBotChat iChat, int iArgument, TEntityType iType, TEntityIndex iIndex )
{
	// Speak.
	m_cTaskStack.push_back( CBorzhTask(EBorzhTaskSpeak, MAKE_TYPE(iChat) | iArgument) );

	// Look at entity before speak (door, button, box, etc).
	if ( iType != EEntityTypeInvalid )
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskLook, MAKE_TYPE(iType) | MAKE_INDEX(iIndex)) );
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::StartPlannerForGoal()
{
	m_cCurrentBigTask.iTask = EBorzhTaskGoToGoal;
	m_cCurrentBigTask.iArgument = 0;
	m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitPlanner) );
	TaskFinish();

	BotMessage("%s -> StartPlannerForGoal(): starting planner.", GetName());
	CPlanner::Lock(this);
	CPlanner::Start(this);
}

//----------------------------------------------------------------------------------------------------------------
bool CBotBorzh::CheckButtonDoorConfigurations()
{
	TEntityIndex iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);
	TEntityIndex iDoor = GET_DOOR(m_cCurrentBigTask.iArgument);

	// Increment previous button/door.
	if ( iDoor == 0xFF )
	{
		iButton++;
		iDoor = 0;
	}
	else
		iDoor++;

	const good::vector<CEntity>& aDoors = CItems::GetItems(EEntityTypeDoor);
	const good::vector<CEntity>& aButtons = CItems::GetItems(EEntityTypeButton);

	int iDoorsSize = aDoors.size();
	int iButtonsSize = aButtons.size();

	bool bFinished;
	do {
		bFinished = true;

		// Skip buttons that toggles 2 doors or the ones that toggle some door and not toggle the rest.
		while ( (iButton < iButtonsSize) && 
		        ( !m_cSeenButtons.test(iButton) ||
		          (m_cButtonTogglesDoor[iButton].count() > 1) ||
		          (m_cButtonTogglesDoor[iButton].count() + m_cButtonNoAffectDoor[iButton].count() == iDoorsSize) ) )
		{
			iButton++;
			iDoor = 0;
		}
		if ( iButton == iButtonsSize )
			break;

		if ( !m_cPushedButtons.test(iButton) )
		{
			iDoor = EEntityIndexInvalid;
			break;
		}

		// Skip doors that are not discovered yet or button-door configuration is known. Skip tested configurations.
		while ( (iDoor < iDoorsSize) && (!m_cSeenDoors.test(iDoor) || m_cButtonTogglesDoor[iButton].test(iDoor) || 
		        m_cButtonNoAffectDoor[iButton].test(iDoor) || m_cTestedToggles[iButton].test(iDoor)) )
			iDoor++;

		if ( iDoor == iDoorsSize )
		{
			iButton++;
			iDoor = 0;
			bFinished = (iButton == iButtonsSize);
		}
	} while ( !bFinished );

	if ( iButton == iButtonsSize ) // All tested.
	{
		BigTaskFinish();
		m_bUsedPlannerForButton = true;
		// Start again next time.
		for ( int i = 0; i < m_cTestedToggles.size(); ++i )
			m_cTestedToggles[i].reset();
		BotMessage("%s -> CheckButtonDoorConfigurations(): no more button-door configs to test. Reset next time.", GetName());
		return false;
	}

	m_cCurrentBigTask.iTask = EBorzhTaskButtonDoorConfig;
	m_cCurrentBigTask.iArgument = 0;
	SET_BUTTON(iButton, m_cCurrentBigTask.iArgument);
	SET_DOOR(iDoor, m_cCurrentBigTask.iArgument);

	m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitPlanner, 10000) ); // TODO: m_iPlannerWaitTime.
	TaskFinish();

	if ( iDoor != EEntityIndexInvalid )
		m_cTestedToggles[iButton].set(iDoor);
	BotMessage( "%s -> CheckButtonDoorConfigurations(): button%d - door%d (door0 = only button).", GetName(), iButton+1, iDoor+1 );
	BotMessage("%s -> CheckButtonDoorConfigurations(): starting planner.", GetName());

	CPlanner::Start(this);

	return true;
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::DoSpeakTask( int iArgument )
{
	m_cChat.iBotChat = GET_TYPE(iArgument);
	m_cChat.cMap.clear();
	m_cChat.iDirectedTo = GET_PLAYER(iArgument);

	switch ( m_cChat.iBotChat )
	{
	case EBorzhChatDoorFound:
	case EBorzhChatDoorChange:
	case EBorzhChatDoorNoChange:
		// Add door status.
		m_cChat.cMap.push_back( CChatVarValue(CModBorzh::iVarDoorStatus, 0, GET_DOOR_STATUS(iArgument)) );
		// Don't break to add door index.

	case EBorzhChatDoorTry:
	case EBorzhChatDoorGo:
		m_cChat.cMap.push_back( CChatVarValue(CModBorzh::iVarDoor, 0, GET_DOOR(iArgument) ) );
		break;

	case EBorzhChatSeeButton:
	case EBorzhChatButtonIPush:
	case EBorzhChatButtonYouPush:
	case EBorzhChatButtonIShoot:
	case EBorzhChatButtonYouShoot:
	case EBorzhChatButtonTry:
	case EBorzhChatButtonGo:
	case EBorzhChatButtonTryGo:
		m_cChat.cMap.push_back( CChatVarValue(CModBorzh::iVarButton, 0, GET_BUTTON(iArgument) ) );
		break;

	case EBorzhChatButtonDoor:
	case EBorzhChatButtonToggles:
	case EBorzhChatButtonNoToggles:
		m_cChat.cMap.push_back( CChatVarValue(CModBorzh::iVarButton, 0, GET_BUTTON(iArgument) ) );
		m_cChat.cMap.push_back( CChatVarValue(CModBorzh::iVarDoor, 0, GET_DOOR(iArgument) ) );
		break;

	case EBorzhChatWeaponFound:
		m_cChat.cMap.push_back( CChatVarValue(CModBorzh::iVarWeapon, 0, GET_WEAPON(iArgument)) );
		break;

	case EBorzhChatNewArea:
	case EBorzhChatChangeArea:
	case EBorzhChatAreaCantGo:
	case EBorzhChatAreaGo:
		m_cChat.cMap.push_back( CChatVarValue(CModBorzh::iVarArea, 0, GET_AREA(iArgument)) );
		break;
	}

	Speak(false);
	Wait(m_iTimeAfterSpeak);
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::BigTaskFinish()
{
	BotMessage("%s -> BigTaskFinish(): %s", GetName(), CTypeToString::BorzhTaskToString(m_cCurrentBigTask.iTask).c_str());
	CancelTasksInStack();
	if ( IsPlannerTask() )
	{
		CPlanner::Stop();
		CPlanner::Unlock(this);
	}
	m_cCurrentBigTask.iTask = EBorzhTaskInvalid;
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::PushPressButtonTask( TEntityIndex iButton, bool bShoot )
{
	DebugAssert( !bShoot || m_cPlayersWithCrossbow.test(m_iIndex) );

	// Push/shoot button.
	if ( bShoot )
	{
		// Remove zoom.
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWait, m_iTimeAfterPushingButton) );
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWeaponRemoveZoom, iButton) );

		// Shoot.
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWeaponShoot, iButton) );

		// Look at button again for better precision and wait.
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWait, 1000) );
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskLook, MAKE_TYPE(EEntityTypeButton) | MAKE_INDEX(iButton)) );

		// Zoom.
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWeaponZoom, iButton) );

		// Set crossbow weapon.
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWeaponSet, CModBorzh::iVarValueWeaponCrossbow) );
	}
	else
	{
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskPushButton, iButton) );

		// Look at button again for better precision and wait.
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWait, 1000) );
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskLook, MAKE_TYPE(EEntityTypeButton) | MAKE_INDEX(iButton)) );
	}

	// Say: I will push/shoot button $button now.
	m_cTaskStack.push_back( CBorzhTask(EBorzhTaskSpeak, MAKE_TYPE(bShoot ? EBorzhChatButtonIShoot : EBorzhChatButtonIPush) | MAKE_BUTTON(iButton)) );

	// Look at button.
	m_cTaskStack.push_back( CBorzhTask(EBorzhTaskLook, MAKE_TYPE(EEntityTypeButton) | MAKE_INDEX(iButton)) );

	// Go to button waypoint.
	const CEntity& cButton = CItems::GetItems(EEntityTypeButton)[iButton];
	int iArgument;
	if ( bShoot )
		iArgument = CModBorzh::GetWaypointToShootButton(iButton);
	else
		iArgument = cButton.iWaypoint;
	m_cTaskStack.push_back( CBorzhTask(EBorzhTaskMove, iArgument) );
	TaskFinish();
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::PushCheckButtonTask( TEntityIndex iButton, bool bShoot )
{
	DebugAssert( !bShoot || m_cPlayersWithCrossbow.test(m_iIndex) );

	// Mark all doors as unchecked.
	m_cCheckedDoors.reset();

	m_cCurrentBigTask.iTask = EBorzhTaskButtonTry;
	m_cCurrentBigTask.iArgument = 0;
	SET_BUTTON(iButton, m_cCurrentBigTask.iArgument);
	SET_DOOR(0xFF, m_cCurrentBigTask.iArgument);

	//DebugAssert( m_cCurrentProposedTask.iTask == EBorzhTaskInvalid );
	m_cCurrentProposedTask = m_cCurrentBigTask;

	PushPressButtonTask(iButton, bShoot);
	OfferCollaborativeTask();
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::CheckAcceptedPlayersForCollaborativeTask()
{
	DebugAssert( IsCollaborativeTask() );
	if ( m_cAcceptedPlayers.count() == m_cCollaborativePlayers.count() )
	{
		SET_ACCEPTED(m_cCurrentBigTask.iArgument); // Start executing plan or press button.
		TaskFinish();
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::OfferCollaborativeTask() // TODO: OfferCollaborativeTask()
{
	DebugAssert( IsCollaborativeTask() );
	SET_ASKED_FOR_HELP(m_cCurrentBigTask.iArgument);

	m_cAcceptedPlayers.reset();
	m_cAcceptedPlayers.set(m_iIndex);

	SaveCurrentTask();

	// Wait for answer after speak.
	m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitAnswer, m_iTimeToWaitPlayer) );

	// Speak.
	if ( m_cCurrentBigTask.iTask == EBorzhTaskButtonTry )
	{
		int iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);

		// Say: Let's try to find which doors opens button $button.
		PushSpeakTask(EBorzhChatButtonTry, MAKE_BUTTON(iButton));
	}
	else if ( m_cCurrentBigTask.iTask == EBorzhTaskButtonDoorConfig )
	{
		DebugAssert( CPlanner::GetPlan() );
		int iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);
		int iDoor = GET_DOOR(m_cCurrentBigTask.iArgument);

		// Say: Let's try to check if button $button toggles door $door.
		if ( iDoor == 0xFF )
			PushSpeakTask(EBorzhChatButtonTryGo, MAKE_BUTTON(iButton));
		else
			PushSpeakTask(EBorzhChatButtonDoor, MAKE_BUTTON(iButton) | MAKE_DOOR(iDoor));
	}
	else
	{
		DebugAssert(false);
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::ReceiveTaskOffer( TBorzhTask iProposedTask, int iArgument1, int iArgument2, TPlayerIndex iSpeaker )
{
	if ( (iProposedTask == EBorzhTaskButtonDoorConfigHelp) && (iArgument2 != EEntityIndexInvalid) )
	{
		// Check if bot knows if button affects door.
		if ( m_cButtonTogglesDoor[iArgument1].test(iArgument2) )
		{
			SwitchToSpeakTask( EBorzhChatButtonToggles, MAKE_BUTTON(iArgument1) | MAKE_DOOR(iArgument2) );
			return;
		}
		else if ( m_cButtonNoAffectDoor[iArgument1].test(iArgument2) )
		{
			SwitchToSpeakTask( EBorzhChatButtonNoToggles, MAKE_BUTTON(iArgument1) | MAKE_DOOR(iArgument2) );
			return;
		}
	}

	// Accept task if there is nothing to do or something to do with less priority.
	bool bAcceptTask = false, bNewTask = false;
	if ( m_cCurrentBigTask.iTask == EBorzhTaskInvalid )
	{
		bAcceptTask = CheckForNewTasks(iProposedTask);
		bNewTask = true;
	}
	else if ( m_cCurrentBigTask.iTask < iProposedTask ) // Proposed task has more priority.
		bAcceptTask = true;

	if ( bAcceptTask )
	{
		// Cancel current task.
		if ( IsPlannerTask() )
		{
			BotMessage("%s -> ReceiveTaskOffer(): cancelling %s, button%d-door%d", GetName(), CTypeToString::BorzhTaskToString(m_cCurrentBigTask.iTask).c_str(),
			               GET_BUTTON(m_cCurrentBigTask.iArgument)+1, GET_DOOR(m_cCurrentBigTask.iArgument)+1); 
			CPlanner::Stop();
			CPlanner::Unlock(this);
		}
		CancelTasksInStack();
		TaskFinish();
		m_bNothingToDo = false;

		m_cCurrentBigTask.iTask = iProposedTask;
		switch ( iProposedTask )
		{
			case EBorzhTaskButtonTryHelp:
				BotMessage( "%s -> Accepted task %s, button %d.", GetName(), CTypeToString::BorzhTaskToString(iProposedTask).c_str(), iArgument1+1 );
				// iArgument is button index.
				m_cCurrentBigTask.iArgument = 0;
				SET_BUTTON(iArgument1, m_cCurrentBigTask.iArgument);
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitButton, iArgument1) ); // Wait for button to be pushed.
				PushSpeakTask(EBorzhChatOk); // Accept the task to check doors after pushing button.
				break;

			case EBorzhTaskButtonDoorConfigHelp:
				BotMessage( "%s -> Accepted task %s, button %d, door %d.", GetName(), CTypeToString::BorzhTaskToString(iProposedTask).c_str(), iArgument1+1, iArgument2+1 );
				m_cCurrentBigTask.iArgument = 0;
				SET_BUTTON(iArgument1, m_cCurrentBigTask.iArgument);
				SET_DOOR(iArgument2, m_cCurrentBigTask.iArgument);
				SET_PLAYER(iSpeaker, m_cCurrentBigTask.iArgument);

				PushSpeakTask( (rand()&1) ? EBorzhChatOk : EBotChatAffirmative );
				break;

			default:
				DebugAssert(false);
		}
	}
	else
	{
		BotMessage( "%s -> task %s rejected, current one is %s.", GetName(), 
		            CTypeToString::BorzhTaskToString(iProposedTask).c_str(), CTypeToString::BorzhTaskToString(m_cCurrentBigTask.iTask).c_str() );
		// Say: wait.
		//SwitchToSpeakTask( (rand()&1) ? EBorzhChatWait : EBotChatBusy, MAKE_PLAYER(iSpeaker) );
		if ( bNewTask )
			SwitchToSpeakTask(EBorzhChatBetterIdea, MAKE_PLAYER(iSpeaker));
		else
			SwitchToSpeakTask(EBorzhChatWait, MAKE_PLAYER(iSpeaker));
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::ButtonPushed( TEntityIndex iButton )
{
	m_cCheckedDoors.reset();
	m_cPushedButtons.set(iButton);

	if ( IsTryingButton() )
		SET_PUSHED(m_cCurrentBigTask.iArgument); // Save that button was pushed.

	m_cVisitedAreasAfterPushButton.reset();
	m_cVisitedAreasAfterPushButton.set(m_aPlayersAreas[m_iIndex]);

	const good::bitset& toggles = m_cButtonTogglesDoor[iButton];
	if ( toggles.any() )
	{
		for ( int iDoor = 0; iDoor < toggles.size(); ++iDoor )
			if ( toggles.test(iDoor) )
				m_cOpenedDoors.set( iDoor, !m_cOpenedDoors.test(iDoor) );
	}

	// Recalculate reachable areas.
	SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);
}


//----------------------------------------------------------------------------------------------------------------
bool CBotBorzh::IsPlannerTaskFinished()
{
	DebugAssert( IsPlannerTask() || IsPlannerTaskHelp() );
	if ( (m_cCurrentBigTask.iTask == EBorzhTaskGoToGoal) || (m_cCurrentBigTask.iTask == EBorzhTaskGoToGoalHelp) )
	{
	}
	else if ( (m_cCurrentBigTask.iTask == EBorzhTaskButtonDoorConfig) || (m_cCurrentBigTask.iTask == EBorzhTaskButtonDoorConfigHelp) )
	{
		TEntityIndex iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);
		TEntityIndex iDoor = GET_DOOR(m_cCurrentBigTask.iArgument);

		TWaypointId iButtonWaypoint = CItems::GetItems(EEntityTypeButton)[iButton].iWaypoint;
		if ( iButtonWaypoint == EWaypointIdInvalid ) // Button is for shoot.
			iButtonWaypoint = CModBorzh::GetWaypointToShootButton(iButton);
		DebugAssert( CWaypoints::IsValid(iButtonWaypoint) );

		TAreaId iButtonArea = CWaypoints::Get(iButtonWaypoint).iAreaId;
		TAreaId iDoorArea1 = EAreaIdInvalid;
		TAreaId iDoorArea2 = EAreaIdInvalid;

		TPlayerIndex iPlayerAtButton = EEntityIndexInvalid;
		TPlayerIndex iPlayerAtDoor = EEntityIndexInvalid;

		for ( int i = 0; (i < m_cCollaborativePlayers.size()) && (iPlayerAtButton == EEntityIndexInvalid) && 
						((iDoor == 0xFF) || (iPlayerAtDoor == EEntityIndexInvalid)); ++i )
		{
			if ( m_cCollaborativePlayers.test(i) )
			{
				TAreaId iPlayerArea = m_aPlayersAreas[i];
				if ( iPlayerArea == iButtonArea )
					iPlayerAtButton = i;
				else if ( iPlayerArea == iDoorArea1 || iPlayerArea == iDoorArea2 )
					iPlayerAtDoor = i;
			}
		}
		return (iPlayerAtButton != EEntityIndexInvalid) && ((iDoor == 0xFF) || (iPlayerAtDoor == EEntityIndexInvalid));
	}
	else
		DebugAssert(false);
	return true;
}

//----------------------------------------------------------------------------------------------------------------
bool CBotBorzh::PlanStepNext()
{
	DebugAssert( !CPlanner::IsRunning() );
	int iStep = GET_STEP(m_cCurrentBigTask.iArgument);
	const CPlanner::CPlan* pPlan = CPlanner::GetPlan();
	DebugAssert( pPlan && iStep <= pPlan->size() );
	if ( iStep == pPlan->size() )
	{
		// All plan steps performed, end big task.
		BigTaskFinish();
		return false;
	}
	else
	{
		SET_STEP(iStep + 1, m_cCurrentBigTask.iArgument); // Increment to next step.

		const CAction& cAction = pPlan->at(iStep);

		BotMessage( "%s -> step %d, action %s, argument %d, performer %s.", GetName(), iStep, CTypeToString::BotActionToString(cAction.iAction).c_str(),
			cAction.iArgument, CPlayers::Get(cAction.iExecutioner)->GetName() );

		PlanStepExecute(cAction);
		return true;
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::PlanStepExecute( const CAction& cAction )
{
	switch ( cAction.iAction )
	{
		case EBotActionMove:
			if ( cAction.iExecutioner == m_iIndex )
			{
				TWaypointId iWaypoint = CModBorzh::GetRandomAreaWaypoint(cAction.iArgument);
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskMove, iWaypoint) );
				//PushSpeakTask(EBorzhChatAreaIGo, MAKE_AREA(cAction.iArgument));
			}
			else
			{
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitPlayer, cAction.iExecutioner) );
				PushSpeakTask(EBorzhChatAreaGo, MAKE_AREA(cAction.iArgument) | MAKE_PLAYER(cAction.iExecutioner));
			}
			TaskFinish();
			break;

		case EBotActionPushButton:
		case EBotActionShootButton:
		{
			bool bShoot = (CItems::GetItems(EEntityTypeButton)[cAction.iArgument].iWaypoint == EWaypointIdInvalid);
			DebugAssert( bShoot || (cAction.iAction == EBotActionPushButton) );
			if ( cAction.iExecutioner == m_iIndex )
			{
				PushPressButtonTask(cAction.iArgument, bShoot);
			}
			else
			{
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitPlayer, cAction.iExecutioner) );
				PushSpeakTask(bShoot ? EBorzhChatButtonYouShoot : EBorzhChatButtonYouPush, MAKE_BUTTON(cAction.iArgument) | MAKE_PLAYER(cAction.iExecutioner));
			}
			TaskFinish();
			break;
		}

		case EBotActionCarryBox:
			break;

		case EBotActionCarryBoxFar:
			break;

		case EBotActionDropBox:
			break;

		case EBotActionClimbBox:
			break;

		case EBotActionClimbBot:
			break;

		case EBotActionClimbBoxBot:
			break;
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::PlanStepLast()
{
	if ( m_cCurrentBigTask.iTask == EBorzhTaskButtonDoorConfig )
	{
		TEntityIndex iDoor = GET_DOOR(m_cCurrentBigTask.iArgument);
		if ( iDoor == 0xFF )
			return; // One bot is already at area of new button, so it will set new task to check that button.

		// Move one bot to button and another bot to door.
		TEntityIndex iButton = GET_BUTTON(m_cCurrentBigTask.iArgument);
		const CEntity& cButton = CItems::GetItems(EEntityTypeButton)[iButton];
		bool bShoot = !CWaypoints::IsValid(cButton.iWaypoint);
		TWaypointId iWaypointButton = bShoot ? CModBorzh::GetWaypointToShootButton(iButton) : cButton.iWaypoint;
		DebugAssert( CWaypoints::IsValid(iWaypointButton) );
		TAreaId iAreaButton = CWaypoints::Get(iWaypointButton).iAreaId;

		const CEntity& cDoor = CItems::GetItems(EEntityTypeDoor)[iDoor];
		TWaypointId iWaypointDoor1 = cDoor.iWaypoint;
		TWaypointId iWaypointDoor2 = (TWaypointId)cDoor.pArguments;
		DebugAssert( CWaypoints::IsValid(iWaypointDoor1) && CWaypoints::IsValid(iWaypointDoor2) );
		TAreaId iAreaDoor1 = CWaypoints::Get(iWaypointDoor1).iAreaId;
		TAreaId iAreaDoor2 = CWaypoints::Get(iWaypointDoor2).iAreaId;

		TPlayerIndex iButtonPlayer = EPlayerIndexInvalid, iDoorPlayer = EPlayerIndexInvalid;

		// Check which bot presses the button and which goes to the door.
		for ( TPlayerIndex iPlayer = 0; iPlayer < CPlayers::Size() && (iButtonPlayer == EPlayerIndexInvalid || iDoorPlayer == EPlayerIndexInvalid); ++iPlayer )
		{
			if ( m_cCollaborativePlayers.test(iPlayer) )
			{
				TAreaId iArea = m_aPlayersAreas[iPlayer];
				if ( (iButtonPlayer == EPlayerIndexInvalid) && (iArea == iAreaButton) )
					iButtonPlayer = iPlayer;
				if ( (iDoorPlayer == EPlayerIndexInvalid) && ( (iArea == iAreaDoor1) || (iArea == iAreaDoor2) ) )
					iDoorPlayer = iPlayer;
			}
		}

		DebugAssert( (iButtonPlayer != EPlayerIndexInvalid) && (iDoorPlayer != EPlayerIndexInvalid) );

		// Press button or say to another player to do it. After this or another player goes to the door.
		if ( iButtonPlayer == m_iIndex )
			PushPressButtonTask(iButton, bShoot);
		else
		{
			m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitPlayer, iButtonPlayer) );
			PushSpeakTask( EBorzhChatButtonYouPush, MAKE_BUTTON(iButton) | MAKE_PLAYER(iButtonPlayer) );

			//m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitPlayer, iButtonPlayer) );
			//PushSpeakTask( EBorzhChatButtonGo, MAKE_BUTTON(iButton) | MAKE_PLAYER(iButtonPlayer) );
		}

		// Go to the door or say to another player to do it.
		if ( iDoorPlayer == m_iIndex )
		{
			// Wait for another player to push the button.
			m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitButton, iButton) );
			// Move to door waypoint.
			TAreaId iArea = m_aPlayersAreas[iDoorPlayer];
			m_cTaskStack.push_back( CBorzhTask(EBorzhTaskMove, (iArea == iAreaDoor1) ? iWaypointDoor1 : iWaypointDoor2) );
		}
		else
		{
			m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitPlayer, iButtonPlayer) );
			PushSpeakTask(EBorzhChatDoorGo, MAKE_DOOR(iDoor) | MAKE_PLAYER(iDoorPlayer) );
		}
		TaskFinish();
	}
	else
	{
		DebugAssert(EBorzhTaskGoToGoal);
		// GOAL IS REACHED!!!
		DebugAssert(false);
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::UnexpectedChatForCollaborativeTask( TPlayerIndex iSpeaker, bool bWaitForThisPlayer )
{
	DebugAssert( IsCollaborativeTask() && m_cCollaborativePlayers.test(iSpeaker) );
	if ( IS_ASKED_FOR_HELP(m_cCurrentBigTask.iArgument) )
	{
		if ( !IS_ACCEPTED(m_cCurrentBigTask.iArgument) || (GetPlanStepPerformer() == iSpeaker) )
		{
			CancelCollaborativeTask( bWaitForThisPlayer ? iSpeaker : EPlayerIndexInvalid );
		}
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBotBorzh::CancelCollaborativeTask( TPlayerIndex iWaitForPlayer )
{
	DebugAssert( IsCollaborativeTask() || IsPlannerTaskHelp() );

	// It is collaborative task, cancel it and wait until player is free.
	CancelTasksInStack();
	TaskFinish();
	if ( IsPlannerTask() )
	{
		BotMessage("%s -> CancelCollaborativeTask(): cancelling planner for button%d-door%d", GetName(),
		               GET_BUTTON(m_cCurrentBigTask.iArgument)+1, GET_DOOR(m_cCurrentBigTask.iArgument)+1); 
		CPlanner::Stop();
		CPlanner::Unlock(this);
	}

	// Say task is cancelled.
	if ( IsCollaborativeTask() && IS_ACCEPTED(m_cCurrentBigTask.iArgument) )
		PushSpeakTask(EBorzhChatTaskCancel);
	
	// Wait for player if this player is busy right now.
	if ( iWaitForPlayer != EPlayerIndexInvalid )
	{
		m_cBusyPlayers.set(iWaitForPlayer);
		m_bNothingToDo = true;
	}

	m_cCurrentBigTask.iTask = EBorzhTaskInvalid;
}
