#include "global.h"
#include "Profile.h"
#include "RageUtil.h"
#include "PrefsManager.h"
#include "XmlFile.h"
#include "IniFile.h"
#include "GameManager.h"
#include "RageLog.h"
#include "Song.h"
#include "SongManager.h"
#include "Steps.h"
#include "Course.h"
#include "ThemeManager.h"
#include "CryptManager.h"
#include "ProfileManager.h"
#include "RageFile.h"
#include "RageFileDriverDeflate.h"
#include "RageFileManager.h"
#include "LuaManager.h"
#include "UnlockManager.h"
#include "XmlFile.h"
#include "XmlFileUtil.h"
#include "Foreach.h"
#include "CatalogXml.h"
#include "Bookkeeper.h"
#include "Game.h"
#include "CharacterManager.h"
#include "Character.h"

const RString STATS_XSL            = "Stats.xsl";
const RString COMMON_XSL           = "Common.xsl";
const RString STATS_XML            = "Stats.xml";
const RString STATS_XML_GZ         = "Stats.xml.gz";
const RString EDITABLE_INI         = "Editable.ini";
const RString DONT_SHARE_SIG       = "DontShare.sig";
const RString PUBLIC_KEY_FILE      = "public.key";
const RString SCREENSHOTS_SUBDIR   = "Screenshots/";
const RString EDIT_STEPS_SUBDIR    = "Edits/";
const RString EDIT_COURSES_SUBDIR  = "EditCourses/";

ThemeMetric<bool> SHOW_COIN_DATA( "Profile", "ShowCoinData" );
static Preference<bool> g_bProfileDataCompress( "ProfileDataCompress", false );
static Preference<bool> g_bCopyCatalogToProfiles( "CopyCatalogToProfiles", true );
extern Preference<bool> g_bWriteCatalog;
static ThemeMetric<RString> UNLOCK_AUTH_STRING( "Profile", "UnlockAuthString" );
#define GUID_SIZE_BYTES 8

#define MAX_EDITABLE_INI_SIZE_BYTES			2*1024		// 2KB
#define MAX_PLAYER_STATS_XML_SIZE_BYTES	\
	100 /* Songs */						\
	* 3 /* Steps per Song */			\
	* 10 /* HighScores per Steps */		\
	* 1024 /* size in bytes of a HighScores XNode */

const unsigned int DEFAULT_WEIGHT_POUNDS	= 120;

#if defined(_MSC_VER)
#pragma warning (disable : 4706) // assignment within conditional expression
#endif


int Profile::HighScoresForASong::GetNumTimesPlayed() const
{
	int iCount = 0;
	FOREACHM_CONST( StepsID, HighScoresForASteps, m_StepsHighScores, i )
	{
		iCount += i->second.hsl.GetNumTimesPlayed();
	}
	return iCount;
}

int Profile::HighScoresForACourse::GetNumTimesPlayed() const
{
	int iCount = 0;
	FOREACHM_CONST( TrailID, HighScoresForATrail, m_TrailHighScores, i )
	{
		iCount += i->second.hsl.GetNumTimesPlayed();
	}
	return iCount;
}


void Profile::InitEditableData()
{
	m_sDisplayName = "";
	m_sCharacterID = "";
	m_sLastUsedHighScoreName = "";
	m_iWeightPounds = 0;
}

void Profile::ClearStats()
{
	// don't reset the Guid
	RString sGuid = m_sGuid;
	InitAll();
	m_sGuid = sGuid;
}

RString Profile::MakeGuid()
{
	RString s;
	s.reserve( GUID_SIZE_BYTES*2 );
	unsigned char buf[GUID_SIZE_BYTES];
	CryptManager::GetRandomBytes( buf, GUID_SIZE_BYTES );
	for( unsigned i=0; i<GUID_SIZE_BYTES; i++ )
		s += ssprintf( "%02x", buf[i] );
	return s;
}

void Profile::InitGeneralData()
{
	m_sGuid = MakeGuid();

	m_SortOrder = SortOrder_Invalid;
	m_LastDifficulty = Difficulty_Invalid;
	m_LastCourseDifficulty = Difficulty_Invalid;
	m_LastStepsType = StepsType_Invalid;
	m_lastSong.Unset();
	m_lastCourse.Unset();
	m_iTotalPlays = 0;
	m_iTotalPlaySeconds = 0;
	m_iTotalGameplaySeconds = 0;
	m_fTotalCaloriesBurned = 0;
	m_GoalType = (GoalType)0;
	m_iGoalCalories = 0;
	m_iGoalSeconds = 0;
	m_iTotalDancePoints = 0;
	m_iNumExtraStagesPassed = 0;
	m_iNumExtraStagesFailed = 0;
	m_iNumToasties = 0;
	m_UnlockedEntryIDs.clear();
	m_sLastPlayedMachineGuid = "";
	m_LastPlayedDate.Init();
	m_iTotalTapsAndHolds = 0;
	m_iTotalJumps = 0;
	m_iTotalHolds = 0;
	m_iTotalRolls = 0;
	m_iTotalMines = 0;
	m_iTotalHands = 0;

	FOREACH_ENUM( PlayMode, i )
		m_iNumSongsPlayedByPlayMode[i] = 0;
	m_iNumSongsPlayedByStyle.clear();
	FOREACH_ENUM( Difficulty, i )
		m_iNumSongsPlayedByDifficulty[i] = 0;
	for( int i=0; i<MAX_METER+1; i++ )
		m_iNumSongsPlayedByMeter[i] = 0;
	m_iNumTotalSongsPlayed = 0;
	ZERO( m_iNumStagesPassedByPlayMode );
	ZERO( m_iNumStagesPassedByGrade );
}

void Profile::InitSongScores()
{
	m_SongHighScores.clear();
}

void Profile::InitCourseScores()
{
	m_CourseHighScores.clear();
}

void Profile::InitCategoryScores()
{
	FOREACH_ENUM( StepsType,st )
		FOREACH_ENUM( RankingCategory,rc )
			m_CategoryHighScores[st][rc].Init();
}

void Profile::InitScreenshotData()
{
	m_vScreenshots.clear();
}

void Profile::InitCalorieData()
{
	m_mapDayToCaloriesBurned.clear();
}

void Profile::InitRecentSongScores()
{
	m_vRecentStepsScores.clear();
}

void Profile::InitRecentCourseScores()
{
	m_vRecentCourseScores.clear();
}

RString Profile::GetDisplayNameOrHighScoreName() const
{
	if( !m_sDisplayName.empty() )
		return m_sDisplayName;
	else if( !m_sLastUsedHighScoreName.empty() )
		return m_sLastUsedHighScoreName;
	else
		return RString();
}

Character *Profile::GetCharacter() const
{
	vector<Character*> vpCharacters;
	CHARMAN->GetCharacters( vpCharacters );
	FOREACH_CONST( Character*, vpCharacters, c )
	{
		if( (*c)->m_sCharacterID.CompareNoCase(m_sCharacterID)==0 )
			return *c;
	}
	return CHARMAN->GetDefaultCharacter();
}

static RString FormatCalories( float fCals )
{
	return Commify((int)fCals) + " Cal";
}

int Profile::GetCalculatedWeightPounds() const
{
	if( m_iWeightPounds == 0 )	// weight not entered
		return DEFAULT_WEIGHT_POUNDS;
	else 
		return m_iWeightPounds;
}

RString Profile::GetDisplayTotalCaloriesBurned() const
{
	return FormatCalories( m_fTotalCaloriesBurned );
}

RString Profile::GetDisplayTotalCaloriesBurnedToday() const
{
	float fCals = GetCaloriesBurnedToday();
	return FormatCalories( fCals );
}

float Profile::GetCaloriesBurnedToday() const
{
	DateTime now = DateTime::GetNowDate();
	return GetCaloriesBurnedForDay(now);
}

int Profile::GetTotalNumSongsPassed() const
{
	int iTotal = 0;
	FOREACH_ENUM( PlayMode, i )
		iTotal += m_iNumStagesPassedByPlayMode[i];
	return iTotal;
}

int Profile::GetTotalStepsWithTopGrade( StepsType st, Difficulty d, Grade g ) const
{
	int iCount = 0;

	FOREACH_CONST( Song*, SONGMAN->GetSongs(), pSong )
	{
		if( !(*pSong)->NormallyDisplayed() )
			continue;	// skip

		FOREACH_CONST( Steps*, (*pSong)->GetAllSteps(), pSteps )
		{
			if( (*pSteps)->m_StepsType != st )
				continue;	// skip

			if( (*pSteps)->GetDifficulty() != d )
				continue;	// skip

			const HighScoreList &hsl = GetStepsHighScoreList( *pSong, *pSteps );
			if( hsl.vHighScores.empty() )
				continue;	// skip

			if( hsl.vHighScores[0].GetGrade() == g )
				iCount++;
		}
	}

	return iCount;
}

int Profile::GetTotalTrailsWithTopGrade( StepsType st, CourseDifficulty d, Grade g ) const
{
	int iCount = 0;

	// add course high scores
	vector<Course*> vCourses;
	SONGMAN->GetAllCourses( vCourses, false );
	FOREACH_CONST( Course*, vCourses, pCourse )
	{
		// Don't count any course that has any entries that change over time.
		if( !(*pCourse)->AllSongsAreFixed() )
			continue;

		vector<Trail*> vTrails;
		Trail* pTrail = (*pCourse)->GetTrail( st, d );
		if( pTrail == NULL )
			continue;

		const HighScoreList &hsl = GetCourseHighScoreList( *pCourse, pTrail );
		if( hsl.vHighScores.empty() )
			continue;	// skip

		if( hsl.vHighScores[0].GetGrade() == g )
			iCount++;
	}

	return iCount;
}

float Profile::GetSongsPossible( StepsType st, Difficulty dc ) const
{
	int iTotalSteps = 0;

	// add steps high scores
	const vector<Song*> &vSongs = SONGMAN->GetSongs();
	for( unsigned i=0; i<vSongs.size(); i++ )
	{
		Song* pSong = vSongs[i];
		
		if( !pSong->NormallyDisplayed() )
			continue;	// skip

		vector<Steps*> vSteps = pSong->GetAllSteps();
		for( unsigned j=0; j<vSteps.size(); j++ )
		{
			Steps* pSteps = vSteps[j];
			
			if( pSteps->m_StepsType != st )
				continue;	// skip

			if( pSteps->GetDifficulty() != dc )
				continue;	// skip

			iTotalSteps++;
		}
	}

	return (float) iTotalSteps;
}

float Profile::GetSongsActual( StepsType st, Difficulty dc ) const
{
	CHECKPOINT_M( ssprintf("Profile::GetSongsActual(%d,%d)",st,dc) );
	
	float fTotalPercents = 0;
	
	// add steps high scores
	FOREACHM_CONST( SongID, HighScoresForASong, m_SongHighScores, i )
	{
		const SongID &id = i->first;
		Song* pSong = id.ToSong();
		
		CHECKPOINT_M( ssprintf("Profile::GetSongsActual: %p", pSong) );
		
		// If the Song isn't loaded on the current machine, then we can't 
		// get radar values to compute dance points.
		if( pSong == NULL )
			continue;
		
		if( !pSong->NormallyDisplayed() )
			continue;	// skip
		
		CHECKPOINT_M( ssprintf("Profile::GetSongsActual: song %s", pSong->GetSongDir().c_str()) );
		const HighScoresForASong &hsfas = i->second;
		
		FOREACHM_CONST( StepsID, HighScoresForASteps, hsfas.m_StepsHighScores, j )
		{
			const StepsID &id = j->first;
			Steps* pSteps = id.ToSteps( pSong, true );
			CHECKPOINT_M( ssprintf("Profile::GetSongsActual: song %p, steps %p", pSong, pSteps) );
			
			// If the Steps isn't loaded on the current machine, then we can't 
			// get radar values to compute dance points.
			if( pSteps == NULL )
				continue;
			
			if( pSteps->m_StepsType != st )
				continue;
			
			CHECKPOINT_M( ssprintf("Profile::GetSongsActual: n %s = %p", id.ToString().c_str(), pSteps) );
			if( pSteps->GetDifficulty() != dc )
				continue;	// skip
			CHECKPOINT;
			
			const HighScoresForASteps& h = j->second;
			const HighScoreList& hsl = h.hsl;
			
			fTotalPercents += hsl.GetTopScore().GetPercentDP();
		}
		CHECKPOINT;
	}

	return fTotalPercents;
}

float Profile::GetSongsPercentComplete( StepsType st, Difficulty dc ) const
{
	return GetSongsActual(st,dc) / GetSongsPossible(st,dc);
}

static void GetHighScoreCourses( vector<Course*> &vpCoursesOut )
{
	vpCoursesOut.clear();

	vector<Course*> vpCourses;
	SONGMAN->GetAllCourses( vpCourses, false );
	FOREACH_CONST( Course*, vpCourses, c )
	{
		// Don't count any course that has any entries that change over time.
		if( !(*c)->AllSongsAreFixed() )
			continue;

		vpCoursesOut.push_back( *c );
	}
}

float Profile::GetCoursesPossible( StepsType st, CourseDifficulty cd ) const
{
	int iTotalTrails = 0;

	vector<Course*> vpCourses;
	GetHighScoreCourses( vpCourses );
	FOREACH_CONST( Course*, vpCourses, c )
	{
		Trail* pTrail = (*c)->GetTrail(st,cd);
		if( pTrail == NULL )
			continue;

		iTotalTrails++;
	}
	
	return (float) iTotalTrails;
}
	
float Profile::GetCoursesActual( StepsType st, CourseDifficulty cd ) const
{
	float fTotalPercents = 0;
	
	vector<Course*> vpCourses;
	GetHighScoreCourses( vpCourses );
	FOREACH_CONST( Course*, vpCourses, c )
	{
		Trail *pTrail = (*c)->GetTrail( st, cd );
		if( pTrail == NULL )
			continue;

		const HighScoreList& hsl = GetCourseHighScoreList( *c, pTrail );
		fTotalPercents += hsl.GetTopScore().GetPercentDP();
	}

	return fTotalPercents;
}

float Profile::GetCoursesPercentComplete( StepsType st, CourseDifficulty cd ) const
{
	return GetCoursesActual(st,cd) / GetCoursesPossible(st,cd);
}

float Profile::GetSongsAndCoursesPercentCompleteAllDifficulties( StepsType st ) const
{
	float fActual = 0;
	float fPossible = 0;
	FOREACH_ENUM( Difficulty, d )
	{
		fActual += GetSongsActual(st,d);
		fPossible += GetSongsPossible(st,d);
	}
	FOREACH_ENUM( CourseDifficulty, d )
	{
		fActual += GetCoursesActual(st,d);
		fPossible += GetCoursesPossible(st,d);
	}
	return fActual / fPossible;
}

int Profile::GetSongNumTimesPlayed( const Song* pSong ) const
{
	SongID songID;
	songID.FromSong( pSong );
	return GetSongNumTimesPlayed( songID );
}

int Profile::GetSongNumTimesPlayed( const SongID& songID ) const
{
	const HighScoresForASong *hsSong = GetHighScoresForASong( songID );
	if( hsSong == NULL )
		return 0;

	int iTotalNumTimesPlayed = 0;
	FOREACHM_CONST( StepsID, HighScoresForASteps, hsSong->m_StepsHighScores, j )
	{
		const HighScoresForASteps &hsSteps = j->second;

		iTotalNumTimesPlayed += hsSteps.hsl.GetNumTimesPlayed();
	}
	return iTotalNumTimesPlayed;
}

/*
 * Get the profile default modifiers.  Return true if set, in which case sModifiersOut
 * will be set.  Return false if no modifier string is set, in which case the theme
 * defaults should be used.  Note that the null string means "no modifiers are active",
 * which is distinct from no modifier string being set at all.
 *
 * In practice, we get the default modifiers from the theme the first time a game
 * is played, and from the profile every time thereafter.
 */
bool Profile::GetDefaultModifiers( const Game* pGameType, RString &sModifiersOut ) const
{
	map<RString,RString>::const_iterator it;
	it = m_sDefaultModifiers.find( pGameType->m_szName );
	if( it == m_sDefaultModifiers.end() )
		return false;
	sModifiersOut = it->second;
	return true;
}

void Profile::SetDefaultModifiers( const Game* pGameType, const RString &sModifiers )
{
	if( sModifiers == "" )
		m_sDefaultModifiers.erase( pGameType->m_szName );
	else
		m_sDefaultModifiers[pGameType->m_szName] = sModifiers;
}

bool Profile::IsCodeUnlocked( RString sUnlockEntryID ) const
{
	return m_UnlockedEntryIDs.find( sUnlockEntryID ) != m_UnlockedEntryIDs.end();
}


Song *Profile::GetMostPopularSong() const
{
	int iMaxNumTimesPlayed = 0;
	SongID id;
	FOREACHM_CONST( SongID, HighScoresForASong, m_SongHighScores, i )
	{
		int iNumTimesPlayed = i->second.GetNumTimesPlayed();
		if( iNumTimesPlayed > iMaxNumTimesPlayed )
		{
			id = i->first;
			iMaxNumTimesPlayed = iNumTimesPlayed;
		}
	}

	return id.ToSong();
}

Course *Profile::GetMostPopularCourse() const
{
	int iMaxNumTimesPlayed = 0;
	CourseID id;
	FOREACHM_CONST( CourseID, HighScoresForACourse, m_CourseHighScores, i )
	{
		int iNumTimesPlayed = i->second.GetNumTimesPlayed();
		if( iNumTimesPlayed > iMaxNumTimesPlayed )
		{
			id = i->first;
			iMaxNumTimesPlayed = iNumTimesPlayed;
		}
	}

	return id.ToCourse();
}

//
// Steps high scores
//
void Profile::AddStepsHighScore( const Song* pSong, const Steps* pSteps, HighScore hs, int &iIndexOut )
{
	GetStepsHighScoreList(pSong,pSteps).AddHighScore( hs, iIndexOut, IsMachine() );
}

const HighScoreList& Profile::GetStepsHighScoreList( const Song* pSong, const Steps* pSteps ) const
{
	return ((Profile*)this)->GetStepsHighScoreList(pSong,pSteps);
}

HighScoreList& Profile::GetStepsHighScoreList( const Song* pSong, const Steps* pSteps )
{
	SongID songID;
	songID.FromSong( pSong );
	
	StepsID stepsID;
	stepsID.FromSteps( pSteps );
	
	HighScoresForASong &hsSong = m_SongHighScores[songID];	// operator[] inserts into map
	HighScoresForASteps &hsSteps = hsSong.m_StepsHighScores[stepsID];	// operator[] inserts into map

	return hsSteps.hsl;
}

int Profile::GetStepsNumTimesPlayed( const Song* pSong, const Steps* pSteps ) const
{
	return GetStepsHighScoreList(pSong,pSteps).GetNumTimesPlayed();
}

DateTime Profile::GetSongLastPlayedDateTime( const Song* pSong ) const
{
	SongID id;
	id.FromSong( pSong );
	std::map<SongID,HighScoresForASong>::const_iterator iter = m_SongHighScores.find( id );

	// don't call this unless has been played once
	ASSERT( iter != m_SongHighScores.end() );
	ASSERT( !iter->second.m_StepsHighScores.empty() );

	DateTime dtLatest;	// starts out zeroed
	FOREACHM_CONST( StepsID, HighScoresForASteps, iter->second.m_StepsHighScores, i )
	{
		const HighScoreList &hsl = i->second.hsl;
		if( hsl.GetNumTimesPlayed() == 0 )
			continue;
		if( dtLatest < hsl.GetLastPlayed() )
			dtLatest = hsl.GetLastPlayed();
	}
	return dtLatest;
}

bool Profile::HasPassedSteps( const Song* pSong, const Steps* pSteps ) const
{
	const HighScoreList &hsl = GetStepsHighScoreList( pSong, pSteps );
	Grade grade = hsl.GetTopScore().GetGrade();
	switch( grade )
	{
	case Grade_Failed:
	case Grade_NoData:
		return false;
	default:
		return true;
	}
}

bool Profile::HasPassedAnyStepsInSong( const Song* pSong ) const
{
	FOREACH_CONST( Steps*, pSong->GetAllSteps(), steps )
	{
		if( HasPassedSteps( pSong, *steps ) )
			return true;
	}
	return false;
}

void Profile::IncrementStepsPlayCount( const Song* pSong, const Steps* pSteps )
{
	DateTime now = DateTime::GetNowDate();
	GetStepsHighScoreList(pSong,pSteps).IncrementPlayCount( now );
}

void Profile::GetGrades( const Song* pSong, StepsType st, int iCounts[NUM_Grade] ) const
{
	SongID songID;
	songID.FromSong( pSong );

	
	memset( iCounts, 0, sizeof(int)*NUM_Grade );
	const HighScoresForASong *hsSong = GetHighScoresForASong( songID );
	if( hsSong == NULL )
		return;

	FOREACH_ENUM( Grade,g)
	{
		FOREACHM_CONST( StepsID, HighScoresForASteps, hsSong->m_StepsHighScores, it )
		{
			const StepsID &id = it->first;
			if( !id.MatchesStepsType(st) )
				continue;

			const HighScoresForASteps &hsSteps = it->second;
			if( hsSteps.hsl.GetTopScore().GetGrade() == g )
				iCounts[g]++;
		}
	}
}

//
// Course high scores
//
void Profile::AddCourseHighScore( const Course* pCourse, const Trail* pTrail, HighScore hs, int &iIndexOut )
{
	GetCourseHighScoreList(pCourse,pTrail).AddHighScore( hs, iIndexOut, IsMachine() );
}

const HighScoreList& Profile::GetCourseHighScoreList( const Course* pCourse, const Trail* pTrail ) const
{
	return ((Profile *)this)->GetCourseHighScoreList( pCourse, pTrail );
}

HighScoreList& Profile::GetCourseHighScoreList( const Course* pCourse, const Trail* pTrail )
{
	CourseID courseID;
	courseID.FromCourse( pCourse );

	TrailID trailID;
	trailID.FromTrail( pTrail );

	HighScoresForACourse &hsCourse = m_CourseHighScores[courseID];	// operator[] inserts into map
	HighScoresForATrail &hsTrail = hsCourse.m_TrailHighScores[trailID];	// operator[] inserts into map

	return hsTrail.hsl;
}

int Profile::GetCourseNumTimesPlayed( const Course* pCourse ) const
{
	CourseID courseID;
	courseID.FromCourse( pCourse );

	return GetCourseNumTimesPlayed( courseID );
}

int Profile::GetCourseNumTimesPlayed( const CourseID &courseID ) const
{
	const HighScoresForACourse *hsCourse = GetHighScoresForACourse( courseID );
	if( hsCourse == NULL )
		return 0;

	int iTotalNumTimesPlayed = 0;
	FOREACHM_CONST( TrailID, HighScoresForATrail, hsCourse->m_TrailHighScores, j )
	{
		const HighScoresForATrail &hsTrail = j->second;

		iTotalNumTimesPlayed += hsTrail.hsl.GetNumTimesPlayed();
	}
	return iTotalNumTimesPlayed;
}

DateTime Profile::GetCourseLastPlayedDateTime( const Course* pCourse ) const
{
	CourseID id;
	id.FromCourse( pCourse );
	std::map<CourseID,HighScoresForACourse>::const_iterator iter = m_CourseHighScores.find( id );

	// don't call this unless has been played once
	ASSERT( iter != m_CourseHighScores.end() );
	ASSERT( !iter->second.m_TrailHighScores.empty() );

	DateTime dtLatest;	// starts out zeroed
	FOREACHM_CONST( TrailID, HighScoresForATrail, iter->second.m_TrailHighScores, i )
	{
		const HighScoreList &hsl = i->second.hsl;
		if( hsl.GetNumTimesPlayed() == 0 )
			continue;
		if( dtLatest < hsl.GetLastPlayed() )
			dtLatest = hsl.GetLastPlayed();
	}
	return dtLatest;
}

void Profile::IncrementCoursePlayCount( const Course* pCourse, const Trail* pTrail )
{
	DateTime now = DateTime::GetNowDate();
	GetCourseHighScoreList(pCourse,pTrail).IncrementPlayCount( now );
}

//
// Category high scores
//
void Profile::AddCategoryHighScore( StepsType st, RankingCategory rc, HighScore hs, int &iIndexOut )
{
	m_CategoryHighScores[st][rc].AddHighScore( hs, iIndexOut, IsMachine() );
}

const HighScoreList& Profile::GetCategoryHighScoreList( StepsType st, RankingCategory rc ) const
{
	return ((Profile *)this)->m_CategoryHighScores[st][rc];
}

HighScoreList& Profile::GetCategoryHighScoreList( StepsType st, RankingCategory rc )
{
	return m_CategoryHighScores[st][rc];
}

int Profile::GetCategoryNumTimesPlayed( StepsType st ) const
{
	int iNumTimesPlayed = 0;
	FOREACH_ENUM( RankingCategory,rc )
		iNumTimesPlayed += m_CategoryHighScores[st][rc].GetNumTimesPlayed();
	return iNumTimesPlayed;
}

void Profile::IncrementCategoryPlayCount( StepsType st, RankingCategory rc )
{
	DateTime now = DateTime::GetNowDate();
	m_CategoryHighScores[st][rc].IncrementPlayCount( now );
}


//
// Loading and saving
//

#define WARN_PARSER	ShowWarningOrTrace( __FILE__, __LINE__, "Error parsing file.", true )
#define WARN_AND_RETURN { WARN_PARSER; return; }
#define WARN_AND_CONTINUE { WARN_PARSER; continue; }
#define WARN_AND_BREAK { WARN_PARSER; break; }
#define WARN_M(m)	ShowWarningOrTrace( __FILE__, __LINE__, RString("Error parsing file: ")+(m), true )
#define WARN_AND_RETURN_M(m) { WARN_M(m); return; }
#define WARN_AND_CONTINUE_M(m) { WARN_M(m); continue; }
#define WARN_AND_BREAK_M(m) { WARN_M(m); break; }
#define LOAD_NODE(X)	{ \
	const XNode* X = xml->GetChild(#X); \
	if( X==NULL ) LOG->Warn("Failed to read section " #X); \
	else Load##X##FromNode(X); }

ProfileLoadResult Profile::LoadAllFromDir( RString sDir, bool bRequireSignature )
{
	CHECKPOINT;

	LOG->Trace( "Profile::LoadAllFromDir( %s )", sDir.c_str() );

	ASSERT( sDir.Right(1) == "/" );

	InitAll();

	// Not critical if this fails
	LoadEditableDataFromDir( sDir );
	
	// Check for the existance of stats.xml
	RString fn = sDir + STATS_XML;
	bool bCompressed = false;
	if( !IsAFile(fn) )
	{
		// Check for the existance of stats.xml.gz
		fn = sDir + STATS_XML_GZ;
		bCompressed = true;
		if( !IsAFile(fn) )
			return ProfileLoadResult_FailedNoProfile;
	}

	int iError;
	auto_ptr<RageFileBasic> pFile( FILEMAN->Open(fn, RageFile::READ, iError) );
	if( pFile.get() == NULL )
	{
		LOG->Trace( "Error opening %s: %s", fn.c_str(), strerror(iError) );
		return ProfileLoadResult_FailedTampered;
	}

	if( bCompressed )
	{
		RString sError;
		uint32_t iCRC32;
		RageFileObjInflate *pInflate = GunzipFile( pFile.release(), sError, &iCRC32 );
		if( pInflate == NULL )
		{
			LOG->Trace( "Error opening %s: %s", fn.c_str(), sError.c_str() );
			return ProfileLoadResult_FailedTampered;
		}

		pFile.reset( pInflate );
	}

	//
	// Don't unreasonably large stats.xml files.
	//
	if( !IsMachine() )	// only check stats coming from the player
	{
		int iBytes = pFile->GetFileSize();
		if( iBytes > MAX_PLAYER_STATS_XML_SIZE_BYTES )
		{
			LOG->Warn( "The file '%s' is unreasonably large.  It won't be loaded.", fn.c_str() );
			return ProfileLoadResult_FailedTampered;
		}
	}

	if( bRequireSignature )
	{ 
		RString sStatsXmlSigFile = fn+SIGNATURE_APPEND;
		RString sDontShareFile = sDir + DONT_SHARE_SIG;

		LOG->Trace( "Verifying don't share signature \"%s\" against \"%s\"", sDontShareFile.c_str(), sStatsXmlSigFile.c_str() );
		// verify the stats.xml signature with the "don't share" file
		if( !CryptManager::VerifyFileWithFile(sStatsXmlSigFile, sDontShareFile) )
		{
			LOG->Warn( "The don't share check for '%s' failed.  Data will be ignored.", sStatsXmlSigFile.c_str() );
			return ProfileLoadResult_FailedTampered;
		}
		LOG->Trace( "Done." );

		// verify stats.xml
		LOG->Trace( "Verifying stats.xml signature" );
		if( !CryptManager::VerifyFileWithFile(fn, sStatsXmlSigFile) )
		{
			LOG->Warn( "The signature check for '%s' failed.  Data will be ignored.", fn.c_str() );
			return ProfileLoadResult_FailedTampered;
		}
		LOG->Trace( "Done." );
	}

	LOG->Trace( "Loading %s", fn.c_str() );
	XNode xml;
	if( !XmlFileUtil::LoadFromFileShowErrors(xml, *pFile.get()) )
		return ProfileLoadResult_FailedTampered;
	LOG->Trace( "Done." );

	return LoadStatsXmlFromNode( &xml );
}

ProfileLoadResult Profile::LoadStatsXmlFromNode( const XNode *xml, bool bIgnoreEditable )
{
	/* The placeholder stats.xml file has an <html> tag.  Don't load it, but don't
	 * warn about it. */
	if( xml->GetName() == "html" )
		return ProfileLoadResult_FailedNoProfile;

	if( xml->GetName() != "Stats" )
	{
		WARN_M( xml->GetName() );
		return ProfileLoadResult_FailedTampered;
	}

	/* These are loaded from Editable, so we usually want to ignore them
	 * here. */
	RString sName = m_sDisplayName;
	RString sCharacterID = m_sCharacterID;
	RString sLastUsedHighScoreName = m_sLastUsedHighScoreName;
	int iWeightPounds = m_iWeightPounds;

	LOAD_NODE( GeneralData );
	LOAD_NODE( SongScores );
	LOAD_NODE( CourseScores );
	LOAD_NODE( CategoryScores );
	LOAD_NODE( ScreenshotData );
	LOAD_NODE( CalorieData );
	LOAD_NODE( RecentSongScores );
	LOAD_NODE( RecentCourseScores );
		
	if( bIgnoreEditable )
	{
		m_sDisplayName = sName;
		m_sCharacterID = sCharacterID;
		m_sLastUsedHighScoreName = sLastUsedHighScoreName;
		m_iWeightPounds = iWeightPounds;
	}

	return ProfileLoadResult_Success;
}

bool Profile::SaveAllToDir( RString sDir, bool bSignData ) const
{
	m_sLastPlayedMachineGuid = PROFILEMAN->GetMachineProfile()->m_sGuid;
	m_LastPlayedDate = DateTime::GetNowDate();

	// Save editable.ini
	SaveEditableDataToDir( sDir );

	bool bSaved = SaveStatsXmlToDir( sDir, bSignData );
	
	SaveStatsWebPageToDir( sDir );

	// Empty directories if none exist.
	if( ProfileManager::m_bProfileStepEdits )
		FILEMAN->CreateDir( sDir + EDIT_STEPS_SUBDIR );
	if( ProfileManager::m_bProfileCourseEdits )
		FILEMAN->CreateDir( sDir + EDIT_COURSES_SUBDIR );
	FILEMAN->CreateDir( sDir + SCREENSHOTS_SUBDIR );

	FILEMAN->FlushDirCache( sDir );

	return bSaved;
}

XNode *Profile::SaveStatsXmlCreateNode() const
{
	XNode *xml = new XNode( "Stats" );

	xml->AppendChild( SaveGeneralDataCreateNode() );
	xml->AppendChild( SaveSongScoresCreateNode() );
	xml->AppendChild( SaveCourseScoresCreateNode() );
	xml->AppendChild( SaveCategoryScoresCreateNode() );
	xml->AppendChild( SaveScreenshotDataCreateNode() );
	xml->AppendChild( SaveCalorieDataCreateNode() );
	xml->AppendChild( SaveRecentSongScoresCreateNode() );
	xml->AppendChild( SaveRecentCourseScoresCreateNode() );
	if( SHOW_COIN_DATA.GetValue() && IsMachine() )
		xml->AppendChild( SaveCoinDataCreateNode() );

	return xml;
}

bool Profile::SaveStatsXmlToDir( RString sDir, bool bSignData ) const
{
	LOG->Trace( "SaveStatsXmlToDir: %s", sDir.c_str() );
	auto_ptr<XNode> xml( SaveStatsXmlCreateNode() );
	
	// Save stats.xml
	RString fn = sDir + (g_bProfileDataCompress? STATS_XML_GZ:STATS_XML);

	{
		RString sError;
		RageFile f;
		if( !f.Open(fn, RageFile::WRITE) )
		{
			LOG->Warn( "Couldn't open %s for writing: %s", fn.c_str(), f.GetError().c_str() );
			return false;
		}
	
		if( g_bProfileDataCompress )
		{
			RageFileObjGzip gzip( &f );
			gzip.Start();
			if( !XmlFileUtil::SaveToFile( xml.get(), gzip, STATS_XSL, false ) )
				return false;
			if( gzip.Finish() == -1 )
				return false;

			/* After successfully saving STATS_XML_GZ, remove any stray STATS_XML. */
			if( FILEMAN->IsAFile(sDir + STATS_XML) )
				FILEMAN->Remove( sDir + STATS_XML );
		}
		else
		{
			if( !XmlFileUtil::SaveToFile( xml.get(), f, STATS_XSL, false ) )
				return false;

			/* After successfully saving STATS_XML, remove any stray STATS_XML_GZ. */
			if( FILEMAN->IsAFile(sDir + STATS_XML_GZ) )
				FILEMAN->Remove( sDir + STATS_XML_GZ );
		}
	}

	// Update file cache, or else IsAFile in CryptManager won't see this new file.
	FILEMAN->FlushDirCache( sDir );
	
	if( bSignData )
	{
		RString sStatsXmlSigFile = fn+SIGNATURE_APPEND;
		CryptManager::SignFileToFile(fn, sStatsXmlSigFile);

		// Update file cache, or else IsAFile in CryptManager won't see sStatsXmlSigFile.
		FILEMAN->FlushDirCache( sDir );

		// Save the "don't share" file
		RString sDontShareFile = sDir + DONT_SHARE_SIG;
		CryptManager::SignFileToFile(sStatsXmlSigFile, sDontShareFile);
	}

	return true;
}

void Profile::SaveEditableDataToDir( RString sDir ) const
{
	IniFile ini;

	ini.SetValue( "Editable", "DisplayName",			m_sDisplayName );
	ini.SetValue( "Editable", "CharacterID",			m_sCharacterID );
	ini.SetValue( "Editable", "LastUsedHighScoreName",		m_sLastUsedHighScoreName );
	ini.SetValue( "Editable", "WeightPounds",			m_iWeightPounds );

	ini.WriteFile( sDir + EDITABLE_INI );
}

XNode* Profile::SaveGeneralDataCreateNode() const
{
	XNode* pGeneralDataNode = new XNode( "GeneralData" );

	// TRICKY: These are write-only elements that are normally never read again.
	// This data is required by other apps (like internet ranking), but is 
	// redundant to the game app.
	pGeneralDataNode->AppendChild( "DisplayName",			GetDisplayNameOrHighScoreName() );
	pGeneralDataNode->AppendChild( "CharacterID",			m_sCharacterID );
	pGeneralDataNode->AppendChild( "LastUsedHighScoreName",		m_sLastUsedHighScoreName );
	pGeneralDataNode->AppendChild( "WeightPounds",			m_iWeightPounds );
	pGeneralDataNode->AppendChild( "IsMachine",			IsMachine() );
	pGeneralDataNode->AppendChild( "IsWeightSet",			m_iWeightPounds != 0 );

	pGeneralDataNode->AppendChild( "Guid",				m_sGuid );
	pGeneralDataNode->AppendChild( "SortOrder",			SortOrderToString(m_SortOrder) );
	pGeneralDataNode->AppendChild( "LastDifficulty",		DifficultyToString(m_LastDifficulty) );
	pGeneralDataNode->AppendChild( "LastCourseDifficulty",		DifficultyToString(m_LastCourseDifficulty) );
	if( m_LastStepsType != StepsType_Invalid )
		pGeneralDataNode->AppendChild( "LastStepsType",			GameManager::GetStepsTypeInfo(m_LastStepsType).szName );
	pGeneralDataNode->AppendChild( m_lastSong.CreateNode() );
	pGeneralDataNode->AppendChild( m_lastCourse.CreateNode() );
	pGeneralDataNode->AppendChild( "TotalPlays",			m_iTotalPlays );
	pGeneralDataNode->AppendChild( "TotalPlaySeconds",		m_iTotalPlaySeconds );
	pGeneralDataNode->AppendChild( "TotalGameplaySeconds",		m_iTotalGameplaySeconds );
	pGeneralDataNode->AppendChild( "TotalCaloriesBurned",		m_fTotalCaloriesBurned );
	pGeneralDataNode->AppendChild( "GoalType",			m_GoalType );
	pGeneralDataNode->AppendChild( "GoalCalories",			m_iGoalCalories );
	pGeneralDataNode->AppendChild( "GoalSeconds",			m_iGoalSeconds );
	pGeneralDataNode->AppendChild( "LastPlayedMachineGuid",		m_sLastPlayedMachineGuid );
	pGeneralDataNode->AppendChild( "LastPlayedDate",		m_LastPlayedDate.GetString() );
	pGeneralDataNode->AppendChild( "TotalDancePoints",		m_iTotalDancePoints );
	pGeneralDataNode->AppendChild( "NumExtraStagesPassed",		m_iNumExtraStagesPassed );
	pGeneralDataNode->AppendChild( "NumExtraStagesFailed",		m_iNumExtraStagesFailed );
	pGeneralDataNode->AppendChild( "NumToasties",			m_iNumToasties );
	pGeneralDataNode->AppendChild( "TotalTapsAndHolds",		m_iTotalTapsAndHolds );
	pGeneralDataNode->AppendChild( "TotalJumps",			m_iTotalJumps );
	pGeneralDataNode->AppendChild( "TotalHolds",			m_iTotalHolds );
	pGeneralDataNode->AppendChild( "TotalRolls",			m_iTotalRolls );
	pGeneralDataNode->AppendChild( "TotalMines",			m_iTotalMines );
	pGeneralDataNode->AppendChild( "TotalHands",			m_iTotalHands );

	// Keep declared variables in a very local scope so they aren't 
	// accidentally used where they're not intended.  There's a lot of
	// copying and pasting in this code.

	{
		XNode* pDefaultModifiers = pGeneralDataNode->AppendChild("DefaultModifiers");
		FOREACHM_CONST( RString, RString, m_sDefaultModifiers, it )
			pDefaultModifiers->AppendChild( it->first, it->second );
	}

	{
		XNode* pUnlocks = pGeneralDataNode->AppendChild("Unlocks");
		FOREACHS_CONST( RString, m_UnlockedEntryIDs, it )
		{
			XNode *pEntry = pUnlocks->AppendChild("UnlockEntry");
			RString sUnlockEntry = it->c_str();
			pEntry->AppendAttr( "UnlockEntryID", sUnlockEntry );
			if( !UNLOCK_AUTH_STRING.GetValue().empty() )
			{
				RString sUnlockAuth = BinaryToHex( CRYPTMAN->GetMD5ForString(sUnlockEntry + UNLOCK_AUTH_STRING.GetValue()) );
				pEntry->AppendAttr( "Auth", sUnlockAuth );
			}
		}
	}

	{
		XNode* pNumSongsPlayedByPlayMode = pGeneralDataNode->AppendChild("NumSongsPlayedByPlayMode");
		FOREACH_ENUM( PlayMode, pm )
		{
			/* Don't save unplayed PlayModes. */
			if( !m_iNumSongsPlayedByPlayMode[pm] )
				continue;
			pNumSongsPlayedByPlayMode->AppendChild( PlayModeToString(pm), m_iNumSongsPlayedByPlayMode[pm] );
		}
	}

	{
		XNode* pNumSongsPlayedByStyle = pGeneralDataNode->AppendChild("NumSongsPlayedByStyle");
		FOREACHM_CONST( StyleID, int, m_iNumSongsPlayedByStyle, iter )
		{
			const StyleID &s = iter->first;
			int iNumPlays = iter->second;

			XNode *pStyleNode = s.CreateNode();
			pStyleNode->AppendAttr(XNode::TEXT_ATTRIBUTE, iNumPlays );

			pNumSongsPlayedByStyle->AppendChild( pStyleNode );
		}
	}

	{
		XNode* pNumSongsPlayedByDifficulty = pGeneralDataNode->AppendChild("NumSongsPlayedByDifficulty");
		FOREACH_ENUM( Difficulty, dc )
		{
			if( !m_iNumSongsPlayedByDifficulty[dc] )
				continue;
			pNumSongsPlayedByDifficulty->AppendChild( DifficultyToString(dc), m_iNumSongsPlayedByDifficulty[dc] );
		}
	}

	{
		XNode* pNumSongsPlayedByMeter = pGeneralDataNode->AppendChild("NumSongsPlayedByMeter");
		for( int i=0; i<MAX_METER+1; i++ )
		{
			if( !m_iNumSongsPlayedByMeter[i] )
				continue;
			pNumSongsPlayedByMeter->AppendChild( ssprintf("Meter%d",i), m_iNumSongsPlayedByMeter[i] );
		}
	}

	pGeneralDataNode->AppendChild( "NumTotalSongsPlayed", m_iNumTotalSongsPlayed );

	{
		XNode* pNumStagesPassedByPlayMode = pGeneralDataNode->AppendChild("NumStagesPassedByPlayMode");
		FOREACH_ENUM( PlayMode, pm )
		{
			/* Don't save unplayed PlayModes. */
			if( !m_iNumStagesPassedByPlayMode[pm] )
				continue;
			pNumStagesPassedByPlayMode->AppendChild( PlayModeToString(pm), m_iNumStagesPassedByPlayMode[pm] );
		}
	}

	{
		XNode* pNumStagesPassedByGrade = pGeneralDataNode->AppendChild("NumStagesPassedByGrade");
		FOREACH_ENUM( Grade, g )
		{
			if( !m_iNumStagesPassedByGrade[g] )
				continue;
			pNumStagesPassedByGrade->AppendChild( GradeToString(g), m_iNumStagesPassedByGrade[g] );
		}
	}

	return pGeneralDataNode;
}

ProfileLoadResult Profile::LoadEditableDataFromDir( RString sDir )
{
	RString fn = sDir + EDITABLE_INI;

	//
	// Don't load unreasonably large editable.xml files.
	//
	int iBytes = FILEMAN->GetFileSizeInBytes( fn );
	if( iBytes > MAX_EDITABLE_INI_SIZE_BYTES )
	{
		LOG->Warn( "The file '%s' is unreasonably large.  It won't be loaded.", fn.c_str() );
		return ProfileLoadResult_FailedTampered;
	}

	if( !IsAFile(fn) )
		return ProfileLoadResult_FailedNoProfile;

	IniFile ini;
	ini.ReadFile( fn );

	ini.GetValue( "Editable", "DisplayName",			m_sDisplayName );
	ini.GetValue( "Editable", "CharacterID",			m_sCharacterID );
	ini.GetValue( "Editable", "LastUsedHighScoreName",		m_sLastUsedHighScoreName );
	ini.GetValue( "Editable", "WeightPounds",			m_iWeightPounds );

	// This is data that the user can change, so we have to validate it.
	wstring wstr = RStringToWstring(m_sDisplayName);
	if( wstr.size() > PROFILE_MAX_DISPLAY_NAME_LENGTH )
		wstr = wstr.substr(0, PROFILE_MAX_DISPLAY_NAME_LENGTH);
	m_sDisplayName = WStringToRString(wstr);
	// TODO: strip invalid chars?
	if( m_iWeightPounds != 0 )
		CLAMP( m_iWeightPounds, 20, 1000 );

	return ProfileLoadResult_Success;
}

void Profile::LoadGeneralDataFromNode( const XNode* pNode )
{
	ASSERT( pNode->GetName() == "GeneralData" );

	RString s;
	const XNode* pTemp;

	pNode->GetChildValue( "DisplayName",				m_sDisplayName );
	pNode->GetChildValue( "CharacterID",				m_sCharacterID );
	pNode->GetChildValue( "LastUsedHighScoreName",			m_sLastUsedHighScoreName );
	pNode->GetChildValue( "WeightPounds",				m_iWeightPounds );
	pNode->GetChildValue( "Guid",					m_sGuid );
	pNode->GetChildValue( "SortOrder",				s );	m_SortOrder = StringToSortOrder( s );
	pNode->GetChildValue( "LastDifficulty",				s );	m_LastDifficulty = StringToDifficulty( s );
	pNode->GetChildValue( "LastCourseDifficulty",			s );	m_LastCourseDifficulty = StringToDifficulty( s );
	pNode->GetChildValue( "LastStepsType",				s );	m_LastStepsType = GAMEMAN->StringToStepsType( s );
	pTemp = pNode->GetChild( "Song" );				if( pTemp ) m_lastSong.LoadFromNode( pTemp );
	pTemp = pNode->GetChild( "Course" );				if( pTemp ) m_lastCourse.LoadFromNode( pTemp );
	pNode->GetChildValue( "TotalPlays",				m_iTotalPlays );
	pNode->GetChildValue( "TotalPlaySeconds",			m_iTotalPlaySeconds );
	pNode->GetChildValue( "TotalGameplaySeconds",			m_iTotalGameplaySeconds );
	pNode->GetChildValue( "TotalCaloriesBurned",			m_fTotalCaloriesBurned );
	pNode->GetChildValue( "GoalType",				*ConvertValue<int>(&m_GoalType) );
	pNode->GetChildValue( "GoalCalories",				m_iGoalCalories );
	pNode->GetChildValue( "GoalSeconds",				m_iGoalSeconds );
	pNode->GetChildValue( "LastPlayedMachineGuid",			m_sLastPlayedMachineGuid );
	pNode->GetChildValue( "LastPlayedDate",				s ); m_LastPlayedDate.FromString( s );
	pNode->GetChildValue( "TotalDancePoints",			m_iTotalDancePoints );
	pNode->GetChildValue( "NumExtraStagesPassed",			m_iNumExtraStagesPassed );
	pNode->GetChildValue( "NumExtraStagesFailed",			m_iNumExtraStagesFailed );
	pNode->GetChildValue( "NumToasties",				m_iNumToasties );
	pNode->GetChildValue( "TotalTapsAndHolds",			m_iTotalTapsAndHolds );
	pNode->GetChildValue( "TotalJumps",				m_iTotalJumps );
	pNode->GetChildValue( "TotalHolds",				m_iTotalHolds );
	pNode->GetChildValue( "TotalRolls",				m_iTotalRolls );
	pNode->GetChildValue( "TotalMines",				m_iTotalMines );
	pNode->GetChildValue( "TotalHands",				m_iTotalHands );

	{
		const XNode* pDefaultModifiers = pNode->GetChild("DefaultModifiers");
		if( pDefaultModifiers )
		{
			FOREACH_CONST_Child( pDefaultModifiers, game_type )
			{
				game_type->GetTextValue( m_sDefaultModifiers[game_type->GetName()] );
			}
		}
	}

	{
		const XNode* pUnlocks = pNode->GetChild("Unlocks");
		if( pUnlocks )
		{
			FOREACH_CONST_Child( pUnlocks, unlock )
			{
				RString sUnlockEntryID;
				if( !unlock->GetAttrValue("UnlockEntryID",sUnlockEntryID) )
					continue;

				if( !UNLOCK_AUTH_STRING.GetValue().empty() )
				{
					RString sUnlockAuth;
					if( !unlock->GetAttrValue("Auth", sUnlockAuth) )
						continue;

					RString sExpectedUnlockAuth = BinaryToHex( CRYPTMAN->GetMD5ForString(sUnlockEntryID + UNLOCK_AUTH_STRING.GetValue()) );
					if( sUnlockAuth != sExpectedUnlockAuth )
						continue;
				}

				m_UnlockedEntryIDs.insert( sUnlockEntryID );
			}
		}
	}

	{
		const XNode* pNumSongsPlayedByPlayMode = pNode->GetChild("NumSongsPlayedByPlayMode");
		if( pNumSongsPlayedByPlayMode )
			FOREACH_ENUM( PlayMode, pm )
				pNumSongsPlayedByPlayMode->GetChildValue( PlayModeToString(pm), m_iNumSongsPlayedByPlayMode[pm] );
	}

	{
		const XNode* pNumSongsPlayedByStyle = pNode->GetChild("NumSongsPlayedByStyle");
		if( pNumSongsPlayedByStyle )
		{
			FOREACH_CONST_Child( pNumSongsPlayedByStyle, style )
			{
				if( style->GetName() != "Style" )
					continue;

				StyleID s;
				s.LoadFromNode( style );

				if( !s.IsValid() )
					WARN_AND_CONTINUE;

				style->GetTextValue( m_iNumSongsPlayedByStyle[s] );
			}
		}
	}

	{
		const XNode* pNumSongsPlayedByDifficulty = pNode->GetChild("NumSongsPlayedByDifficulty");
		if( pNumSongsPlayedByDifficulty )
			FOREACH_ENUM( Difficulty, dc )
				pNumSongsPlayedByDifficulty->GetChildValue( DifficultyToString(dc), m_iNumSongsPlayedByDifficulty[dc] );
	}

	{
		const XNode* pNumSongsPlayedByMeter = pNode->GetChild("NumSongsPlayedByMeter");
		if( pNumSongsPlayedByMeter )
			for( int i=0; i<MAX_METER+1; i++ )
				pNumSongsPlayedByMeter->GetChildValue( ssprintf("Meter%d",i), m_iNumSongsPlayedByMeter[i] );
	}

	pNode->GetChildValue("NumTotalSongsPlayed", m_iNumTotalSongsPlayed );

	{
		const XNode* pNumStagesPassedByGrade = pNode->GetChild("NumStagesPassedByGrade");
		if( pNumStagesPassedByGrade )
			FOREACH_ENUM( Grade, g )
				pNumStagesPassedByGrade->GetChildValue( GradeToString(g), m_iNumStagesPassedByGrade[g] );
	}

	{
		const XNode* pNumStagesPassedByPlayMode = pNode->GetChild("NumStagesPassedByPlayMode");
		if( pNumStagesPassedByPlayMode )
			FOREACH_ENUM( PlayMode, pm )
				pNumStagesPassedByPlayMode->GetChildValue( PlayModeToString(pm), m_iNumStagesPassedByPlayMode[pm] );
	
	}

}

void Profile::AddStepTotals( int iTotalTapsAndHolds, int iTotalJumps, int iTotalHolds, int iTotalRolls, int iTotalMines, int iTotalHands, float fCaloriesBurned )
{
	m_iTotalTapsAndHolds += iTotalTapsAndHolds;
	m_iTotalJumps += iTotalJumps;
	m_iTotalHolds += iTotalHolds;
	m_iTotalRolls += iTotalRolls;
	m_iTotalMines += iTotalMines;
	m_iTotalHands += iTotalHands;

	m_fTotalCaloriesBurned += fCaloriesBurned;

	DateTime date = DateTime::GetNowDate();
	m_mapDayToCaloriesBurned[date].fCals += fCaloriesBurned;
}

XNode* Profile::SaveSongScoresCreateNode() const
{
	CHECKPOINT;

	const Profile* pProfile = this;
	ASSERT( pProfile );

	XNode* pNode = new XNode( "SongScores" );

	FOREACHM_CONST( SongID, HighScoresForASong, m_SongHighScores, i )
	{	
		const SongID &songID = i->first;
		const HighScoresForASong &hsSong = i->second;

		// skip songs that have never been played
		if( pProfile->GetSongNumTimesPlayed(songID) == 0 )
			continue;

		XNode* pSongNode = pNode->AppendChild( songID.CreateNode() );

		int jCheck2 = hsSong.m_StepsHighScores.size();
		int jCheck1 = 0;
		FOREACHM_CONST( StepsID, HighScoresForASteps, hsSong.m_StepsHighScores, j )
		{	
			jCheck1++;
			ASSERT( jCheck1 <= jCheck2 );
			const StepsID &stepsID = j->first;
			const HighScoresForASteps &hsSteps = j->second;

			const HighScoreList &hsl = hsSteps.hsl;

			// skip steps that have never been played
			if( hsl.GetNumTimesPlayed() == 0 )
				continue;

			XNode* pStepsNode = pSongNode->AppendChild( stepsID.CreateNode() );

			pStepsNode->AppendChild( hsl.CreateNode() );
		}
	}
	
	return pNode;
}

void Profile::LoadSongScoresFromNode( const XNode* pSongScores )
{
	CHECKPOINT;

	ASSERT( pSongScores->GetName() == "SongScores" );

	FOREACH_CONST_Child( pSongScores, pSong )
	{
		if( pSong->GetName() != "Song" )
			continue;

		SongID songID;
		songID.LoadFromNode( pSong );
		if( !songID.IsValid() )
			continue;

		FOREACH_CONST_Child( pSong, pSteps )
		{
			if( pSteps->GetName() != "Steps" )
				continue;

			StepsID stepsID;
			stepsID.LoadFromNode( pSteps );
			if( !stepsID.IsValid() )
				WARN_AND_CONTINUE;

			const XNode *pHighScoreListNode = pSteps->GetChild("HighScoreList");
			if( pHighScoreListNode == NULL )
				WARN_AND_CONTINUE;
			
			HighScoreList &hsl = m_SongHighScores[songID].m_StepsHighScores[stepsID].hsl;
			hsl.LoadFromNode( pHighScoreListNode );
		}
	}
}


XNode* Profile::SaveCourseScoresCreateNode() const
{
	CHECKPOINT;

	const Profile* pProfile = this;
	ASSERT( pProfile );

	XNode* pNode = new XNode( "CourseScores" );
	
	FOREACHM_CONST( CourseID, HighScoresForACourse, m_CourseHighScores, i )
	{
		const CourseID &courseID = i->first;
		const HighScoresForACourse &hsCourse = i->second;

		// skip courses that have never been played
		if( pProfile->GetCourseNumTimesPlayed(courseID) == 0 )
			continue;

		XNode* pCourseNode = pNode->AppendChild( courseID.CreateNode() );

		FOREACHM_CONST( TrailID, HighScoresForATrail, hsCourse.m_TrailHighScores, j )
		{
			const TrailID &trailID = j->first;
			const HighScoresForATrail &hsTrail = j->second;

			const HighScoreList &hsl = hsTrail.hsl;

			// skip steps that have never been played
			if( hsl.GetNumTimesPlayed() == 0 )
				continue;

			XNode* pTrailNode = pCourseNode->AppendChild( trailID.CreateNode() );

			pTrailNode->AppendChild( hsl.CreateNode() );
		}
	}

	return pNode;
}

void Profile::LoadCourseScoresFromNode( const XNode* pCourseScores )
{
	CHECKPOINT;

	ASSERT( pCourseScores->GetName() == "CourseScores" );

	vector<Course*> vpAllCourses;
	SONGMAN->GetAllCourses( vpAllCourses, true );

	FOREACH_CONST_Child( pCourseScores, pCourse )
	{
		if( pCourse->GetName() != "Course" )
			continue;

		CourseID courseID;
		courseID.LoadFromNode( pCourse );
		if( !courseID.IsValid() )
			WARN_AND_CONTINUE;
		

		// Backward compatability hack to fix importing scores of old style 
		// courses that weren't in group folder but have now been moved into
		// a group folder: 
		// If the courseID doesn't resolve, then take the file name part of sPath
		// and search for matches of just the file name.
		{
			Course *pC = courseID.ToCourse();
			if( pC == NULL )
			{
				RString sDir, sFName, sExt;
				splitpath( courseID.GetPath(), sDir, sFName, sExt );
				RString sFullFileName = sFName + sExt;

				FOREACH_CONST( Course*, vpAllCourses, c )
				{
					RString sOther = (*c)->m_sPath.Right(sFullFileName.size());

					if( sFullFileName.CompareNoCase(sOther) == 0 )
					{
						pC = *c;
						courseID.FromCourse( pC );
						break;
					}
				}
			}
		}


		FOREACH_CONST_Child( pCourse, pTrail )
		{
			if( pTrail->GetName() != "Trail" )
				continue;
			
			TrailID trailID;
			trailID.LoadFromNode( pTrail );
			if( !trailID.IsValid() )
				WARN_AND_CONTINUE;

			const XNode *pHighScoreListNode = pTrail->GetChild("HighScoreList");
			if( pHighScoreListNode == NULL )
				WARN_AND_CONTINUE;
			
			HighScoreList &hsl = m_CourseHighScores[courseID].m_TrailHighScores[trailID].hsl;
			hsl.LoadFromNode( pHighScoreListNode );
		}
	}
}

XNode* Profile::SaveCategoryScoresCreateNode() const
{
	CHECKPOINT;

	const Profile* pProfile = this;
	ASSERT( pProfile );

	XNode* pNode = new XNode( "CategoryScores" );

	FOREACH_ENUM( StepsType,st )
	{
		// skip steps types that have never been played
		if( pProfile->GetCategoryNumTimesPlayed( st ) == 0 )
			continue;

		XNode* pStepsTypeNode = pNode->AppendChild( "StepsType" );
		pStepsTypeNode->AppendAttr( "Type", GameManager::GetStepsTypeInfo(st).szName );

		FOREACH_ENUM( RankingCategory,rc )
		{
			// skip steps types/categories that have never been played
			if( pProfile->GetCategoryHighScoreList(st,rc).GetNumTimesPlayed() == 0 )
				continue;

			XNode* pRankingCategoryNode = pStepsTypeNode->AppendChild( "RankingCategory" );
			pRankingCategoryNode->AppendAttr( "Type", RankingCategoryToString(rc) );

			const HighScoreList &hsl = pProfile->GetCategoryHighScoreList( (StepsType)st, (RankingCategory)rc );

			pRankingCategoryNode->AppendChild( hsl.CreateNode() );
		}
	}

	return pNode;
}

void Profile::LoadCategoryScoresFromNode( const XNode* pCategoryScores )
{
	CHECKPOINT;

	ASSERT( pCategoryScores->GetName() == "CategoryScores" );

	FOREACH_CONST_Child( pCategoryScores, pStepsType )
	{
		if( pStepsType->GetName() != "StepsType" )
			continue;

		RString str;
		if( !pStepsType->GetAttrValue( "Type", str ) )
			WARN_AND_CONTINUE;
		StepsType st = GameManager::StringToStepsType( str );
		if( st == StepsType_Invalid )
			WARN_AND_CONTINUE_M( str );

		FOREACH_CONST_Child( pStepsType, pRadarCategory )
		{
			if( pRadarCategory->GetName() != "RankingCategory" )
				continue;

			if( !pRadarCategory->GetAttrValue( "Type", str ) )
				WARN_AND_CONTINUE;
			RankingCategory rc = StringToRankingCategory( str );
			if( rc == RankingCategory_Invalid )
				WARN_AND_CONTINUE_M( str );

			const XNode *pHighScoreListNode = pRadarCategory->GetChild("HighScoreList");
			if( pHighScoreListNode == NULL )
				WARN_AND_CONTINUE;
			
			HighScoreList &hsl = this->GetCategoryHighScoreList( st, rc );
			hsl.LoadFromNode( pHighScoreListNode );
		}
	}
}

void Profile::SaveStatsWebPageToDir( RString sDir ) const
{
	ASSERT( PROFILEMAN );

	FileCopy( THEME->GetPathO("Profile",STATS_XSL), sDir+STATS_XSL );
	FileCopy( THEME->GetPathO("Profile",COMMON_XSL), sDir+COMMON_XSL );
	if( g_bCopyCatalogToProfiles && g_bWriteCatalog )
	{
		FileCopy( CATALOG_XML_FILE, sDir+CATALOG_XML );
		FileCopy( THEME->GetPathO("Profile",CATALOG_XSL), sDir+CATALOG_XSL );
	}
}

void Profile::SaveMachinePublicKeyToDir( RString sDir ) const
{
	if( PREFSMAN->m_bSignProfileData && IsAFile(CRYPTMAN->GetPublicKeyFileName()) )
		FileCopy( CRYPTMAN->GetPublicKeyFileName(), sDir+PUBLIC_KEY_FILE );
}

void Profile::AddScreenshot( const Screenshot &screenshot )
{
	m_vScreenshots.push_back( screenshot );
}

void Profile::LoadScreenshotDataFromNode( const XNode* pScreenshotData )
{
	CHECKPOINT;

	ASSERT( pScreenshotData->GetName() == "ScreenshotData" );
	FOREACH_CONST_Child( pScreenshotData, pScreenshot )
	{
		if( pScreenshot->GetName() != "Screenshot" )
			WARN_AND_CONTINUE_M( pScreenshot->GetName() );

		Screenshot ss;
		ss.LoadFromNode( pScreenshot );

		m_vScreenshots.push_back( ss );
	}	
}

XNode* Profile::SaveScreenshotDataCreateNode() const
{
	CHECKPOINT;

	const Profile* pProfile = this;
	ASSERT( pProfile );

	XNode* pNode = new XNode( "ScreenshotData" );

	FOREACH_CONST( Screenshot, m_vScreenshots, ss )
	{
		pNode->AppendChild( ss->CreateNode() );
	}

	return pNode;
}

void Profile::LoadCalorieDataFromNode( const XNode* pCalorieData )
{
	CHECKPOINT;

	ASSERT( pCalorieData->GetName() == "CalorieData" );
	FOREACH_CONST_Child( pCalorieData, pCaloriesBurned )
	{
		if( pCaloriesBurned->GetName() != "CaloriesBurned" )
			WARN_AND_CONTINUE_M( pCaloriesBurned->GetName() );

		RString sDate;
		if( !pCaloriesBurned->GetAttrValue("Date",sDate) )
			WARN_AND_CONTINUE;
		DateTime date;
		if( !date.FromString(sDate) )
			WARN_AND_CONTINUE_M( sDate );

		float fCaloriesBurned = 0;

		pCaloriesBurned->GetTextValue(fCaloriesBurned);

		m_mapDayToCaloriesBurned[date].fCals = fCaloriesBurned;
	}	
}

XNode* Profile::SaveCalorieDataCreateNode() const
{
	CHECKPOINT;

	const Profile* pProfile = this;
	ASSERT( pProfile );

	XNode* pNode = new XNode( "CalorieData" );

	FOREACHM_CONST( DateTime, Calories, m_mapDayToCaloriesBurned, i )
	{
		XNode* pCaloriesBurned = pNode->AppendChild( "CaloriesBurned", i->second.fCals );

		pCaloriesBurned->AppendAttr( "Date", i->first.GetString() );
	}

	return pNode;
}

float Profile::GetCaloriesBurnedForDay( DateTime day ) const
{
	day.StripTime();
	map<DateTime,Calories>::const_iterator i = m_mapDayToCaloriesBurned.find( day );
	if( i == m_mapDayToCaloriesBurned.end() )
		return 0;
	else
		return i->second.fCals;
}

XNode* Profile::HighScoreForASongAndSteps::CreateNode() const
{
	XNode* pNode = new XNode( "HighScoreForASongAndSteps" );

	pNode->AppendChild( songID.CreateNode() );
	pNode->AppendChild( stepsID.CreateNode() );
	pNode->AppendChild( hs.CreateNode() );

	return pNode;
}

void Profile::HighScoreForASongAndSteps::LoadFromNode( const XNode* pNode )
{
	Unset();

	ASSERT( pNode->GetName() == "HighScoreForASongAndSteps" );
	const XNode* p;
	if( (p = pNode->GetChild("Song")) )
		songID.LoadFromNode( p );
	if( (p = pNode->GetChild("Steps")) )
		stepsID.LoadFromNode( p );
	if( (p = pNode->GetChild("HighScore")) )
		hs.LoadFromNode( p );
}

void Profile::LoadRecentSongScoresFromNode( const XNode* pRecentSongScores )
{
	CHECKPOINT;

	ASSERT( pRecentSongScores->GetName() == "RecentSongScores" );
	FOREACH_CONST_Child( pRecentSongScores, p )
	{
		if( p->GetName() == "HighScoreForASongAndSteps" )
		{
			HighScoreForASongAndSteps h;
			h.LoadFromNode( p );

			m_vRecentStepsScores.push_back( h );
		}
		else
		{
			WARN_AND_CONTINUE_M( p->GetName() );
		}
	}	
}

XNode* Profile::SaveRecentSongScoresCreateNode() const
{
	CHECKPOINT;

	const Profile* pProfile = this;
	ASSERT( pProfile );

	XNode* pNode = new XNode( "RecentSongScores" );

	FOREACHD_CONST( HighScoreForASongAndSteps, m_vRecentStepsScores, i )
		pNode->AppendChild( i->CreateNode() );

	return pNode;
}

void Profile::AddStepsRecentScore( const Song* pSong, const Steps* pSteps, HighScore hs )
{
	ASSERT( pSong );
	ASSERT( pSteps );
	HighScoreForASongAndSteps h;
	h.songID.FromSong( pSong );
	ASSERT( h.songID.IsValid() );
	h.stepsID.FromSteps( pSteps );
	ASSERT( h.stepsID.IsValid() );
	h.hs = hs;
	m_vRecentStepsScores.push_back( h );

	int iMaxRecentScoresToSave = IsMachine() ? PREFSMAN->m_iMaxRecentScoresForMachine : PREFSMAN->m_iMaxRecentScoresForPlayer;
	int iNumToErase = m_vRecentStepsScores.size() - iMaxRecentScoresToSave;
	if( iNumToErase > 0 )
		m_vRecentStepsScores.erase( m_vRecentStepsScores.begin(), m_vRecentStepsScores.begin() +  iNumToErase );
}


XNode* Profile::HighScoreForACourseAndTrail::CreateNode() const
{
	XNode* pNode = new XNode( "HighScoreForACourseAndTrail" );

	pNode->AppendChild( courseID.CreateNode() );
	pNode->AppendChild( trailID.CreateNode() );
	pNode->AppendChild( hs.CreateNode() );

	return pNode;
}

void Profile::HighScoreForACourseAndTrail::LoadFromNode( const XNode* pNode )
{
	Unset();

	ASSERT( pNode->GetName() == "HighScoreForACourseAndTrail" );
	const XNode* p;
	if( (p = pNode->GetChild("Course")) )
		courseID.LoadFromNode( p );
	if( (p = pNode->GetChild("Trail")) )
		trailID.LoadFromNode( p );
	if( (p = pNode->GetChild("HighScore")) )
		hs.LoadFromNode( p );
}

void Profile::LoadRecentCourseScoresFromNode( const XNode* pRecentCourseScores )
{
	CHECKPOINT;

	ASSERT( pRecentCourseScores->GetName() == "RecentCourseScores" );
	FOREACH_CONST_Child( pRecentCourseScores, p )
	{
		if( p->GetName() == "HighScoreForACourseAndTrail" )
		{
			HighScoreForACourseAndTrail h;
			h.LoadFromNode( p );

			m_vRecentCourseScores.push_back( h );
		}
		else
		{
			WARN_AND_CONTINUE_M( p->GetName() );
		}
	}	
}

XNode* Profile::SaveRecentCourseScoresCreateNode() const
{
	CHECKPOINT;

	const Profile* pProfile = this;
	ASSERT( pProfile );

	XNode* pNode = new XNode( "RecentCourseScores" );

	FOREACHD_CONST( HighScoreForACourseAndTrail, m_vRecentCourseScores, i )
		pNode->AppendChild( i->CreateNode() );

	return pNode;
}

void Profile::AddCourseRecentScore( const Course* pCourse, const Trail* pTrail, HighScore hs )
{
	HighScoreForACourseAndTrail h;
	h.courseID.FromCourse( pCourse );
	h.trailID.FromTrail( pTrail );
	h.hs = hs;
	m_vRecentCourseScores.push_back( h );
	
	int iMaxRecentScoresToSave = IsMachine() ? PREFSMAN->m_iMaxRecentScoresForMachine : PREFSMAN->m_iMaxRecentScoresForPlayer;
	int iNumToErase = m_vRecentCourseScores.size() - iMaxRecentScoresToSave;
	if( iNumToErase > 0 )
		m_vRecentCourseScores.erase( m_vRecentCourseScores.begin(), m_vRecentCourseScores.begin() +  iNumToErase );
}

StepsType Profile::GetLastPlayedStepsType() const
{
	if( m_vRecentStepsScores.empty() )
		return StepsType_Invalid;
	const HighScoreForASongAndSteps &h = m_vRecentStepsScores.back();
	return h.stepsID.GetStepsType();
}

const Profile::HighScoresForASong *Profile::GetHighScoresForASong( const SongID& songID ) const
{
	map<SongID,HighScoresForASong>::const_iterator it;
	it = m_SongHighScores.find( songID );
	if( it == m_SongHighScores.end() )
		return NULL;
	return &it->second;
}

const Profile::HighScoresForACourse *Profile::GetHighScoresForACourse( const CourseID& courseID ) const
{
	map<CourseID,HighScoresForACourse>::const_iterator it;
	it = m_CourseHighScores.find( courseID );
	if( it == m_CourseHighScores.end() )
		return NULL;
	return &it->second;
}

bool Profile::IsMachine() const
{
	// TODO: Think of a better way to handle this
	return this == PROFILEMAN->GetMachineProfile();
}


XNode* Profile::SaveCoinDataCreateNode() const
{
	CHECKPOINT;

	const Profile* pProfile = this;
	ASSERT( pProfile );

	XNode* pNode = new XNode( "CoinData" );

	{
		int coins[NUM_LAST_DAYS];
		BOOKKEEPER->GetCoinsLastDays( coins );
		XNode* p = pNode->AppendChild( "LastDays" );
		for( int i=0; i<NUM_LAST_DAYS; i++ )
			p->AppendChild( LastDayToString(i), coins[i] );
	}
	{
		int coins[NUM_LAST_WEEKS];
		BOOKKEEPER->GetCoinsLastWeeks( coins );
		XNode* p = pNode->AppendChild( "LastWeeks" );
		for( int i=0; i<NUM_LAST_WEEKS; i++ )
			p->AppendChild( LastWeekToString(i), coins[i] );
	}
	{
		int coins[DAYS_IN_WEEK];
		BOOKKEEPER->GetCoinsByDayOfWeek( coins );
		XNode* p = pNode->AppendChild( "DayOfWeek" );
		for( int i=0; i<DAYS_IN_WEEK; i++ )
			p->AppendChild( DayOfWeekToString(i), coins[i] );
	}
	{
		int coins[HOURS_IN_DAY];
		BOOKKEEPER->GetCoinsByHour( coins );
		XNode* p = pNode->AppendChild( "Hour" );
		for( int i=0; i<HOURS_IN_DAY; i++ )
			p->AppendChild( HourInDayToString(i), coins[i] );
	}

	return pNode;
}

void Profile::MoveBackupToDir( RString sFromDir, RString sToDir )
{
	if( FILEMAN->IsAFile(sFromDir + STATS_XML) &&
		FILEMAN->IsAFile(sFromDir+STATS_XML+SIGNATURE_APPEND) )
	{
		FILEMAN->Move( sFromDir+STATS_XML,					sToDir+STATS_XML );
		FILEMAN->Move( sFromDir+STATS_XML+SIGNATURE_APPEND,	sToDir+STATS_XML+SIGNATURE_APPEND );
	}
	else if( FILEMAN->IsAFile(sFromDir + STATS_XML_GZ) &&
		FILEMAN->IsAFile(sFromDir+STATS_XML_GZ+SIGNATURE_APPEND) )
	{
		FILEMAN->Move( sFromDir+STATS_XML_GZ,					sToDir+STATS_XML );
		FILEMAN->Move( sFromDir+STATS_XML_GZ+SIGNATURE_APPEND,	sToDir+STATS_XML+SIGNATURE_APPEND );
	}

	if( FILEMAN->IsAFile(sFromDir + EDITABLE_INI) )
		FILEMAN->Move( sFromDir+EDITABLE_INI,				sToDir+EDITABLE_INI );
	if( FILEMAN->IsAFile(sFromDir + DONT_SHARE_SIG) )
		FILEMAN->Move( sFromDir+DONT_SHARE_SIG,				sToDir+DONT_SHARE_SIG );
}

// lua start
#include "LuaBinding.h"

class LunaProfile: public Luna<Profile>
{
public:
	static int GetDisplayName( T* p, lua_State *L )			{ lua_pushstring(L, p->m_sDisplayName ); return 1; }
	static int GetHighScoreList( T* p, lua_State *L )
	{
		if( LuaBinding::CheckLuaObjectType(L, 1, "Song") )
		{
			const Song *pSong = Luna<Song>::check(L,1);
			const Steps *pSteps = Luna<Steps>::check(L,2);
			HighScoreList &hsl = p->GetStepsHighScoreList( pSong, pSteps );
			hsl.PushSelf( L );
			return 1;
		}
		else if( LuaBinding::CheckLuaObjectType(L, 1, "Course") )
		{
			const Course *pCourse = Luna<Course>::check(L,1);
			const Trail *pTrail = Luna<Trail>::check(L,2);
			HighScoreList &hsl = p->GetCourseHighScoreList( pCourse, pTrail );
			hsl.PushSelf( L );
			return 1;
		}

		luaL_typerror( L, 1, "Song or Course" );
		return 0;
	}

	static int GetCharacter( T* p, lua_State *L )			{ p->GetCharacter()->PushSelf(L); return 1; }
	static int GetWeightPounds( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iWeightPounds ); return 1; }
	static int SetWeightPounds( T* p, lua_State *L )		{ p->m_iWeightPounds = IArg(1); return 0; }
	static int GetGoalType( T* p, lua_State *L )			{ lua_pushnumber(L, p->m_GoalType ); return 1; }
	static int SetGoalType( T* p, lua_State *L )			{ p->m_GoalType = Enum::Check<GoalType>(L, 1); return 0; }
	static int GetGoalCalories( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iGoalCalories ); return 1; }
	static int SetGoalCalories( T* p, lua_State *L )		{ p->m_iGoalCalories = IArg(1); return 0; }
	static int GetGoalSeconds( T* p, lua_State *L )			{ lua_pushnumber(L, p->m_iGoalSeconds ); return 1; }
	static int SetGoalSeconds( T* p, lua_State *L )			{ p->m_iGoalSeconds = IArg(1); return 0; }
	static int GetCaloriesBurnedToday( T* p, lua_State *L )	{ lua_pushnumber(L, p->GetCaloriesBurnedToday() ); return 1; }
	static int GetTotalNumSongsPlayed( T* p, lua_State *L )	{ lua_pushnumber(L, p->m_iNumTotalSongsPlayed ); return 1; }
	static int IsCodeUnlocked( T* p, lua_State *L )			{ lua_pushboolean(L, p->IsCodeUnlocked(SArg(1)) ); return 1; }
	static int GetSongsActual( T* p, lua_State *L )			{ lua_pushnumber(L, p->GetSongsActual(Enum::Check<StepsType>(L, 1),Enum::Check<Difficulty>(L, 2)) ); return 1; }
	static int GetCoursesActual( T* p, lua_State *L )		{ lua_pushnumber(L, p->GetCoursesActual(Enum::Check<StepsType>(L, 1),Enum::Check<Difficulty>(L, 2)) ); return 1; }
	static int GetSongsPossible( T* p, lua_State *L )		{ lua_pushnumber(L, p->GetSongsPossible(Enum::Check<StepsType>(L, 1),Enum::Check<Difficulty>(L, 2)) ); return 1; }
	static int GetCoursesPossible( T* p, lua_State *L )		{ lua_pushnumber(L, p->GetCoursesPossible(Enum::Check<StepsType>(L, 1),Enum::Check<Difficulty>(L, 2)) ); return 1; }
	static int GetSongsPercentComplete( T* p, lua_State *L )	{ lua_pushnumber(L, p->GetSongsPercentComplete(Enum::Check<StepsType>(L, 1),Enum::Check<Difficulty>(L, 2)) ); return 1; }
	static int GetCoursesPercentComplete( T* p, lua_State *L )	{ lua_pushnumber(L, p->GetCoursesPercentComplete(Enum::Check<StepsType>(L, 1),Enum::Check<Difficulty>(L, 2)) ); return 1; }
	static int GetTotalStepsWithTopGrade( T* p, lua_State *L )	{ lua_pushnumber(L, p->GetTotalStepsWithTopGrade(Enum::Check<StepsType>(L, 1),Enum::Check<Difficulty>(L, 2),Enum::Check<Grade>(L, 3)) ); return 1; }
	static int GetTotalTrailsWithTopGrade( T* p, lua_State *L )	{ lua_pushnumber(L, p->GetTotalTrailsWithTopGrade(Enum::Check<StepsType>(L, 1),Enum::Check<Difficulty>(L, 2),Enum::Check<Grade>(L, 3)) ); return 1; }
	static int GetNumTotalSongsPlayed( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iNumTotalSongsPlayed ); return 1; }
	static int GetLastPlayedStepsType( T* p, lua_State *L )		{ lua_pushnumber(L, p->GetLastPlayedStepsType() ); return 1; }
	static int GetSongsAndCoursesPercentCompleteAllDifficulties( T* p, lua_State *L )		{ lua_pushnumber(L, p->GetSongsAndCoursesPercentCompleteAllDifficulties(Enum::Check<StepsType>(L, 1)) ); return 1; }
	static int GetTotalCaloriesBurned( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_fTotalCaloriesBurned ); return 1; }
	static int GetDisplayTotalCaloriesBurned( T* p, lua_State *L )	{ lua_pushstring(L, p->GetDisplayTotalCaloriesBurned() ); return 1; }
	static int GetMostPopularSong( T* p, lua_State *L )
	{
		Song *p2 = p->GetMostPopularSong();
		if( p2 )
			p2->PushSelf(L);
		else
			lua_pushnil( L );
		return 1;
	}
	static int GetMostPopularCourse( T* p, lua_State *L )
	{
		Course *p2 = p->GetMostPopularCourse();
		if( p2 )
			p2->PushSelf(L);
		else
			lua_pushnil( L );
		return 1;
	}
	static int GetSongNumTimesPlayed( T* p, lua_State *L )
	{
		ASSERT( !lua_isnil(L,1) );
		Song *pS = Luna<Song>::check(L,1);
		lua_pushnumber( L, p->GetSongNumTimesPlayed(pS) );
		return 1;
	}
	static int HasPassedAnyStepsInSong( T* p, lua_State *L )
	{
		ASSERT( !lua_isnil(L,1) );
		Song *pS = Luna<Song>::check(L,1);
		lua_pushboolean( L, p->HasPassedAnyStepsInSong(pS) );
		return 1;
	}
	static int GetNumToasties( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iNumToasties ); return 1; }
	static int GetTotalTapsAndHolds( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iTotalTapsAndHolds ); return 1; }
	static int GetTotalJumps( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iTotalJumps ); return 1; }
	static int GetTotalHolds( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iTotalHolds ); return 1; }
	static int GetTotalRolls( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iTotalRolls ); return 1; }
	static int GetTotalMines( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iTotalMines ); return 1; }
	static int GetTotalHands( T* p, lua_State *L )		{ lua_pushnumber(L, p->m_iTotalHands ); return 1; }

	LunaProfile()
	{
		ADD_METHOD( GetDisplayName );
		ADD_METHOD( GetHighScoreList );
		ADD_METHOD( GetCharacter );
		ADD_METHOD( GetWeightPounds );
		ADD_METHOD( SetWeightPounds );
		ADD_METHOD( GetGoalType );
		ADD_METHOD( SetGoalType );
		ADD_METHOD( GetGoalCalories );
		ADD_METHOD( SetGoalCalories );
		ADD_METHOD( GetGoalSeconds );
		ADD_METHOD( SetGoalSeconds );
		ADD_METHOD( GetCaloriesBurnedToday );
		ADD_METHOD( GetTotalNumSongsPlayed );
		ADD_METHOD( IsCodeUnlocked );
		ADD_METHOD( GetSongsActual );
		ADD_METHOD( GetCoursesActual );
		ADD_METHOD( GetSongsPossible );
		ADD_METHOD( GetCoursesPossible );
		ADD_METHOD( GetSongsPercentComplete );
		ADD_METHOD( GetCoursesPercentComplete );
		ADD_METHOD( GetTotalStepsWithTopGrade );
		ADD_METHOD( GetTotalTrailsWithTopGrade );
		ADD_METHOD( GetNumTotalSongsPlayed );
		ADD_METHOD( GetLastPlayedStepsType );
		ADD_METHOD( GetSongsAndCoursesPercentCompleteAllDifficulties );
		ADD_METHOD( GetTotalCaloriesBurned );
		ADD_METHOD( GetDisplayTotalCaloriesBurned );
		ADD_METHOD( GetMostPopularSong );
		ADD_METHOD( GetMostPopularCourse );
		ADD_METHOD( GetSongNumTimesPlayed );
		ADD_METHOD( HasPassedAnyStepsInSong );
		ADD_METHOD( GetNumToasties );
		ADD_METHOD( GetTotalTapsAndHolds );
		ADD_METHOD( GetTotalJumps );
		ADD_METHOD( GetTotalHolds );
		ADD_METHOD( GetTotalRolls );
		ADD_METHOD( GetTotalMines );
		ADD_METHOD( GetTotalHands );
	}
};

LUA_REGISTER_CLASS( Profile )
// lua end


/*
 * (c) 2001-2004 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
