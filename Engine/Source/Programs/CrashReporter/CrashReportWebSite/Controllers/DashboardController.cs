﻿// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Web.Mvc;

using Tools.CrashReporter.CrashReportWebSite.Models;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// 
	/// </summary>
	public struct FCrashMinimal
	{
		/// <summary>
		/// 
		/// </summary>
		public readonly DateTime TimeOfCrash;

		/// <summary>
		///	
		/// </summary>
		public readonly int UserId;


		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Crash">Crash</param>
		public FCrashMinimal( Crash Crash )
		{
			TimeOfCrash = Crash.TimeOfCrash.Value;
			UserId = Crash.UserNameId.Value;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="InTimeOfCrash"></param>
		/// <param name="InUserId"></param>
		public FCrashMinimal( DateTime InTimeOfCrash, int InUserId )
		{
			TimeOfCrash = InTimeOfCrash;
			UserId = InUserId;
		}
	}

	/// <summary>
	/// The controller to handle graphing of crashes per user group over time.
	/// </summary>
	public class DashboardController : Controller
	{
		/// <summary>Fake id for all user groups</summary>
		public static readonly int AllUserGroupId = -1;

		CrashRepository CrashRepository = new CrashRepository();

		/// <summary></summary>
		public int GetIdFromUserGroup( string UserGroup )
		{
			var Group = CrashRepository.Context.UserGroups.Where( X => X.Name.Contains( UserGroup ) ).FirstOrDefault();
			return Group.Id;
		}

		/// <summary></summary>
		public HashSet<int> GetUserIdsFromUserGroup( string UserGroup )
		{
			int UserGroupId = GetIdFromUserGroup( UserGroup );
			return GetUserIdsFromUserGroupId( UserGroupId );
		}

		/// <summary></summary>
		public HashSet<int> GetUserIdsFromUserGroupId( int UserGroupId )
		{
			var UserIds = CrashRepository.Context.Users.Where( X => X.UserGroupId == UserGroupId ).Select( X => X.Id );
			return new HashSet<int>( UserIds );
		}

		/// <summary>
		/// Return a dictionary of crashes per group grouped by week.
		/// </summary>
		/// <param name="Crashes">A set of crashes to interrogate.</param>
		/// <param name="UserGroupId">The id of the user group to interrogate.</param>
		/// <returns>A dictionary of week vs. crash count.</returns>
		public Dictionary<DateTime, int> GetWeeklyCountsByGroup( List<FCrashMinimal> Crashes, int UserGroupId )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(UserGroupId=" + UserGroupId + ")" ) )
			{
				Dictionary<DateTime, int> Results = new Dictionary<DateTime, int>();

				var UsersIds = GetUserIdsFromUserGroupId( UserGroupId );

				// Trim crashes to user group.
				if( UserGroupId != DashboardController.AllUserGroupId )
				{
					Crashes = Crashes.Where( Crash => UsersIds.Contains( Crash.UserId ) ).ToList();
				}

				try
				{
					Results =
					(
						from CrashDetail in Crashes
						group CrashDetail by CrashDetail.TimeOfCrash.AddDays( -(int)CrashDetail.TimeOfCrash.DayOfWeek ).Date into GroupCount
						orderby GroupCount.Key
						select new { Count = GroupCount.Count(), Date = GroupCount.Key }
					).ToDictionary( x => x.Date, y => y.Count );
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetWeeklyCountsByGroup: " + Ex.ToString() );
				}

				return Results;
			}
		}

		/// <summary>
		/// Return a dictionary of crashes per group grouped by day.
		/// </summary>
		/// <param name="Crashes">A set of crashes to interrogate.</param>
		/// <param name="UserGroupId">The id of the user group to interrogate.</param>
		/// <returns>A dictionary of day vs. crash count.</returns>
		public Dictionary<DateTime, int> GetDailyCountsByGroup( List<FCrashMinimal> Crashes, int UserGroupId )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(UserGroupId=" + UserGroupId + ")" ) )
			{
				Dictionary<DateTime, int> Results = new Dictionary<DateTime, int>();

				var UsersIds = GetUserIdsFromUserGroupId( UserGroupId );

				// Trim crashes to user group.
				if( UserGroupId != DashboardController.AllUserGroupId )
				{
					Crashes = Crashes.Where( Crash => UsersIds.Contains( Crash.UserId ) ).ToList();
				}

				try
				{
					Results =
					(
						from CrashDetail in Crashes
						group CrashDetail by CrashDetail.TimeOfCrash.Date into GroupCount
						orderby GroupCount.Key
						select new { Count = GroupCount.Count(), Date = GroupCount.Key }
					).ToDictionary( x => x.Date, y => y.Count );
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetDailyCountsByGroup: " + Ex.ToString() );
				}

				return Results;
			}
		}

		/// <summary>
		/// The main view of the dash board.
		/// </summary>
		/// <returns>A view showing two charts of crashes over time.</returns>
		public ActionResult Index()
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{

				/*
				//Temporary code to update pattern in crashes and buggs.
				DateTime Today11 = DateTime.UtcNow.AddDays(11);
				DateTime AYear = Today11.AddMonths( -12 );

				var CrashList = CrashRepository.Context.Crashes.Where( X => X.TimeOfCrash >= AYear ).Select( X => X );
				var BuggList = CrashRepository.Context.Buggs.Select( X => X );

				int Current = 0;
				foreach(Crash Crash in CrashList)
				{
					if( string.IsNullOrEmpty(Crash.Pattern) )
					{
						continue;
					}
					if( Crash.Pattern[0] != '+' )
					{
						Crash.Pattern = "+" + Crash.Pattern + "+";
					}
					Current++;
					if( Current % 16384 == 0 )
					{
						CrashRepository.SubmitChanges();
					}
				}
				

				foreach(Bugg Bugg in BuggList)
				{
					if( string.IsNullOrEmpty( Bugg.Pattern ) )
					{
						continue;
					}

					if( Bugg.Pattern[0] != '+' )
					{
						Bugg.Pattern = "+" + Bugg.Pattern + "+";
					}
					Current++;
					if( Current % 16384 == 0 )
					{
						CrashRepository.SubmitChanges();
					}
				}
				*/

				DateTime Today = DateTime.UtcNow;
				DateTime AfewMonthsAgo = Today.AddMonths( -6 );

				FAutoScopedLogTimer LogTimerSQL = new FAutoScopedLogTimer( "CrashesFilterByDate", "", "" );
				IQueryable<Crash> Crashes = CrashRepository.FilterByDate( CrashRepository.ListAll(), AfewMonthsAgo, Today );
				var VMinimalCrashes = Crashes.Select( Crash => new { TimeOfCrash = Crash.TimeOfCrash.Value, UserID = Crash.UserNameId.Value } ).ToList();

				List<FCrashMinimal> MinimalCrashes = new List<FCrashMinimal>( VMinimalCrashes.Count );
				foreach( var Anotype in VMinimalCrashes )
				{
					MinimalCrashes.Add( new FCrashMinimal( Anotype.TimeOfCrash, Anotype.UserID ) );
				}
				LogTimerSQL.Dispose();

				int GeneralUserGroupId = CrashRepository.FindOrAddUserGroup( "General" );
				int CoderUserGroupId = CrashRepository.FindOrAddUserGroup( "Coder" );
				int EngineQAUserGroupId = CrashRepository.FindOrAddUserGroup( "EngineQA" );
				int GameQAUserGroupId = CrashRepository.FindOrAddUserGroup( "GameQA" );
				int AnonymousUserGroupId = CrashRepository.FindOrAddUserGroup( "Anonymous" );

				Dictionary<DateTime, int> GeneralResults = GetWeeklyCountsByGroup( MinimalCrashes, GeneralUserGroupId );
				Dictionary<DateTime, int> CoderResults = GetWeeklyCountsByGroup( MinimalCrashes, CoderUserGroupId );
				Dictionary<DateTime, int> EngineQAResults = GetWeeklyCountsByGroup( MinimalCrashes, EngineQAUserGroupId );
				Dictionary<DateTime, int> GameQAResults = GetWeeklyCountsByGroup( MinimalCrashes, GameQAUserGroupId );
				Dictionary<DateTime, int> AnonymousResults = GetWeeklyCountsByGroup( MinimalCrashes, AnonymousUserGroupId );
				Dictionary<DateTime, int> AllResults = GetWeeklyCountsByGroup( MinimalCrashes, AllUserGroupId );

				Dictionary<DateTime, int> DailyGeneralResults = GetDailyCountsByGroup( MinimalCrashes, GeneralUserGroupId );
				Dictionary<DateTime, int> DailyCoderResults = GetDailyCountsByGroup( MinimalCrashes, CoderUserGroupId );
				Dictionary<DateTime, int> DailyEngineQAResults = GetDailyCountsByGroup( MinimalCrashes, EngineQAUserGroupId );
				Dictionary<DateTime, int> DailyGameQAResults = GetDailyCountsByGroup( MinimalCrashes, GameQAUserGroupId );
				Dictionary<DateTime, int> DailyAnonymousResults = GetDailyCountsByGroup( MinimalCrashes, AnonymousUserGroupId );
				Dictionary<DateTime, int> DailyAllResults = GetDailyCountsByGroup( MinimalCrashes, AllUserGroupId );

				string CrashesByWeek = "";

				foreach( KeyValuePair<DateTime, int> Result in AllResults )
				{
					int GeneralCrashes = 0;
					GeneralResults.TryGetValue( Result.Key, out GeneralCrashes );

					int CoderCrashes = 0;
					CoderResults.TryGetValue( Result.Key, out CoderCrashes );

					int EngineQACrashes = 0;
					EngineQAResults.TryGetValue( Result.Key, out EngineQACrashes );

					int GameQACrashes = 0;
					GameQAResults.TryGetValue( Result.Key, out GameQACrashes );

					int AnonymousCrashes = 0;
					AnonymousResults.TryGetValue( Result.Key, out AnonymousCrashes );

					int Year = Result.Key.Year;
					int Month = Result.Key.AddMonths( -1 ).Month;
					if( Result.Key.Month == 13 || Result.Key.Month == 1 )
					{
						Month = 0;
					}

					int Day = Result.Key.Day + 6;

					string Line = "[new Date(" + Year + ", " + Month + ", " + Day + "), " + GeneralCrashes + ", " + CoderCrashes + ", " + EngineQACrashes + ", " + GameQACrashes + ", " + AnonymousCrashes + ", " + Result.Value + "], ";
					CrashesByWeek += Line;
				}

				CrashesByWeek = CrashesByWeek.TrimEnd( ", ".ToCharArray() );

				string CrashesByDay = "";
				foreach( KeyValuePair<DateTime, int> DailyResult in DailyAllResults )
				{
					int GeneralCrashes = 0;
					DailyGeneralResults.TryGetValue( DailyResult.Key, out GeneralCrashes );

					int CoderCrashes = 0;
					DailyCoderResults.TryGetValue( DailyResult.Key, out CoderCrashes );

					int EngineQACrashes = 0;
					DailyEngineQAResults.TryGetValue( DailyResult.Key, out EngineQACrashes );

					int GameQACrashes = 0;
					DailyGameQAResults.TryGetValue( DailyResult.Key, out GameQACrashes );

					int AnonymousCrashes = 0;
					DailyAnonymousResults.TryGetValue( DailyResult.Key, out AnonymousCrashes );

					int Year = DailyResult.Key.Year;
					int Month = DailyResult.Key.AddMonths( -1 ).Month;
					if( DailyResult.Key.Month == 13 || DailyResult.Key.Month == 1 )
					{
						Month = 0;
					}

					int Day = DailyResult.Key.Day;

					string Line = "[new Date(" + Year + ", " + Month + ", " + Day + "), " + GeneralCrashes + ", " + CoderCrashes + ", " + EngineQACrashes + ", " + GameQACrashes + ", " + AnonymousCrashes + ", " + DailyResult.Value + "], ";
					CrashesByDay += Line;
				}

				CrashesByDay = CrashesByDay.TrimEnd( ", ".ToCharArray() );

				return View( "Index", new DashboardViewModel { CrashesByWeek = CrashesByWeek, CrashesByDay = CrashesByDay } );
			}
		}
	}
}