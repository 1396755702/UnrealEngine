﻿// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.Linq.SqlClient;
using System.Data.Linq;
using System.Diagnostics;
using System.Linq;
using System.Linq.Expressions;
using System.Web;

using Naspinski.IQueryableSearch;

using Tools.CrashReporter.CrashReportCommon;
using Tools.CrashReporter.CrashReportWebSite.Controllers;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	

	/// <summary>
	/// A model to talk to the database.
	/// </summary>
	public class CrashRepository
	{	
		/// <summary>
		/// Gets the crash from an id.
		/// </summary>
		/// <param name="Id">The id of the crash we wish to examine.</param>
		/// <returns>The crash with the requested id.</returns>
		public Crash GetCrash( int Id )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(CrashId" + Id + ")" ) )
			{
				var Context = FRepository.Get().Context;
				try
				{
					IQueryable<Crash> Crashes =
					(
						from CrashDetail in Context.Crashes
						where CrashDetail.Id == Id
						select CrashDetail
					);

					return Crashes.FirstOrDefault();
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetCrash: " + Ex.ToString() );
				}

				return null;
			}
		}


		/// <summary>
		/// Sets the UserName and UserGroupName as derived data for a crash.
		/// </summary>
		/// <param name="CrashInstance">An instance of a crash we wish to augment with additional data.</param>
		public void PopulateUserInfo( Crash CrashInstance )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(CrashId=" + CrashInstance.Id + ")" ) )
			{
				var Context = FRepository.Get().Context;

				try
				{
					int UserGroupId = CrashInstance.User.UserGroupId;
					var Result = Context.UserGroups.Where( i => i.Id == UserGroupId ).First();
					CrashInstance.UserGroupName = Result.Name;
				}
				catch( Exception Ex )
				{
					FLogger.WriteException( "PopulateUserInfo: " + Ex.ToString() );
				}
			}
		}

		/// <summary>
		/// List all crashes from most recent to oldest.
		/// </summary>
		/// <returns>A container of all known crashes.</returns>
		public IQueryable<Crash> ListAll()
		{
			return FRepository.Get().Context.Crashes.AsQueryable();
		}

		/// <summary>
		/// Apply a dynamic query to a set of results.
		/// </summary>
		/// <param name="Results">The initial set of results.</param>
		/// <param name="Query">The query to apply.</param>
		/// <returns>A set of results filtered by the query.</returns>
		public IEnumerable<Crash> Search( IEnumerable<Crash> Results, string Query )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(Query=" + Query + ")" ) )
			{
				IEnumerable<Crash> Crashes = null;
				var IntermediateQueryable = Results.AsQueryable();
				try
				{
					string[] Terms = Query.Split( "-, ;+".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
					string TermsToUse = "";
					foreach( string Term in Terms )
					{
						if( !TermsToUse.Contains( Term ) )
						{
							TermsToUse = TermsToUse + "+" + Term;
						}
					}

					// Search the results by the search terms using IQueryable search
					var CrashesQueryable = (IQueryable<Crash>)IntermediateQueryable.Search( TermsToUse.Split( "+".ToCharArray() ) );
					Crashes = CrashesQueryable.AsEnumerable();
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in Search: " + Ex.ToString() );
					Crashes = Results;
				}

				return Crashes;
			}
		}

		/// <summary>
		/// Return a view model containing a sorted list of crashes based on the user input.
		/// </summary>
		/// <param name="FormData">The user input from the client.</param>
		/// <returns>A view model to use to display a list of crashes.</returns>
		/// <remarks>The filtering mechanism works by generating a fluent query to describe the set of crashes we wish to view. Basically, 'Select.Where().Where().Where().Where()' etc.
		/// The filtering is applied in the following order:
		/// 1. Get all crashes.
		/// 2. Filter between the start and end dates.
		/// 3. Apply the search query.
		/// 4. Filter by the branch.
		/// 5. Filter by the game name.
		/// 6. Filter by the type of reports to view.
		/// 7. Filter by the user group.
		/// 8. Sort the results by the sort term.
		/// 9. Take one page of results.</remarks>
		public CrashesViewModel GetResults( FormHelper FormData )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				UsersMapping UniqueUser = null;
				var Context = FRepository.Get().Context;

				IEnumerable<Crash> Results = null;
				int Skip = ( FormData.Page - 1 ) * FormData.PageSize;
				int Take = FormData.PageSize;

				var ResultsAll = ListAll();

				// Filter by Crash Type
				if( FormData.CrashType != "All" )
				{
					switch( FormData.CrashType )
					{
						case "Crashes":
							ResultsAll = ResultsAll.Where( CrashInstance => CrashInstance.CrashType == 1 );
							break;
						case "Assert":
							ResultsAll = ResultsAll.Where( CrashInstance => CrashInstance.CrashType == 2 );
							break;
						case "Ensure":
							ResultsAll = ResultsAll.Where( CrashInstance => CrashInstance.CrashType == 3 );
							break;
						case "CrashesAsserts":
							ResultsAll = ResultsAll.Where( CrashInstance => CrashInstance.CrashType == 1 || CrashInstance.CrashType == 2 );
							break;
					}
				}			

				// Filter by data and get as enumerable.
				Results = FilterByDate( ResultsAll, FormData.DateFrom, FormData.DateTo );

				// Grab Results 
				if( !string.IsNullOrEmpty( FormData.SearchQuery ) )
				{
					string DecodedQuery = HttpUtility.HtmlDecode( FormData.SearchQuery ).ToLower();
					using( FScopedLogTimer LogTimer2 = new FScopedLogTimer( "CrashRepository.GetResults.FindUserFromQuery" + "(Query=" + DecodedQuery + ")" ) )
					{						
						if( !string.IsNullOrEmpty( DecodedQuery ) )
						{
							// Check if we are looking for user name.
							string[] Params = DecodedQuery.Split( new string[] { "user:" }, StringSplitOptions.None );
							if( Params.Length == 2 )
							{
								/*IQueryable<UsersMapping>*/
								// Make sure that type of [dbo].[UsersMapping] is the same as [analyticsdb-01.dmz.epicgames.net].[CrashReport].[dbo].[UsersMapping]
								IEnumerable<UsersMapping> FoundUsers = Context.ExecuteQuery<UsersMapping>
								( @"SELECT * FROM [analyticsdb-01.dmz.epicgames.net].[CrashReport].[dbo].[UsersMapping] WHERE lower(UserName) = {0} OR lower(UserEmail) = {0}", Params[1] );

								foreach( UsersMapping TheUser in FoundUsers )
								{
									UniqueUser = TheUser;
									break;
								}
								if( UniqueUser != null )
								{
									Results = Results.Where( CrashInstance => CrashInstance.EpicAccountId == UniqueUser.EpicAccountId );
								}
								else
								{
									Results = Results.Where( CrashInstance => CrashInstance.EpicAccountId == "SomeValueThatIsNotPresentInTheDatabase" );
								}
							}
							else
							{
								Results = Search( Results, DecodedQuery );
							}
						}
					}
				}

				// Start Filtering the results

				// Filter by BranchName
				if( !string.IsNullOrEmpty( FormData.BranchName ) )
				{
					if( FormData.BranchName.StartsWith( "-" ) )
					{
						Results =
						(
							from CrashDetail in Results
							where !CrashDetail.Branch.Contains( FormData.BranchName.Substring( 1 ) )
							select CrashDetail
						);
					}
					else
					{
						Results =
						(
							from CrashDetail in Results
							where CrashDetail.Branch.Contains( FormData.BranchName )
							select CrashDetail
						);
					}
				}

				// Filter by GameName
				if( !string.IsNullOrEmpty( FormData.GameName ) )
				{
					if( FormData.GameName.StartsWith( "-" ) )
					{
						Results =
						(
							from CrashDetail in Results
							where !CrashDetail.GameName.Contains( FormData.GameName.Substring( 1 ) )
							select CrashDetail
						);
					}
					else
					{
						Results =
						(
							from CrashDetail in Results
							where CrashDetail.GameName.Contains( FormData.GameName )
							select CrashDetail
						);
					}
				}

				// Get UserGroup ResultCounts
				Dictionary<string, int> GroupCounts = GetCountsByGroupFromCrashes( Results );

				// Filter by user group if present
				int UserGroupId;
				if( !string.IsNullOrEmpty( FormData.UserGroup ) )
				{
					UserGroupId = FRepository.Get().FindOrAddGroup( FormData.UserGroup );
				}
				else
				{
					UserGroupId = 1;
				}

				HashSet<int> UserIdsForGroup = FRepository.Get().GetUserIdsFromUserGroupId( UserGroupId );

				using( FScopedLogTimer LogTimer3 = new FScopedLogTimer( "CrashRepository.Results.Users" ) )
				{
					List<Crash> NewResults = new List<Crash>();
					foreach(Crash Crash in Results)
					{
						if( UserIdsForGroup.Contains( Crash.UserNameId.Value ) )
						{
							NewResults.Add( Crash );
						}
					}

					Results = NewResults;
				}

				// Pass in the results and return them sorted properly
				Results = GetSortedResults( Results, FormData.SortTerm, ( FormData.SortOrder == "Descending" ) );

				// Get the Count for pagination
				int ResultCount = 0;
				using( FScopedLogTimer LogTimer3 = new FScopedLogTimer( "CrashRepository.Results.Users" ) )
				{
					ResultCount = Results.Count();
				}
				

				// Grab just the results we want to display on this page
				Results = Results.Skip( Skip ).Take( Take );

				using( FScopedLogTimer LogTimer3 = new FScopedLogTimer( "CrashRepository.GetResults.GetCallstacks" ) )
				{
					// Process call stack for display
					foreach( Crash CrashInstance in Results )
					{
						// Put callstacks into an list so we can access them line by line in the view
						CrashInstance.CallStackContainer = GetCallStack( CrashInstance );
					}
				}

				return new CrashesViewModel
				{
					Results = Results,
					PagingInfo = new PagingInfo { CurrentPage = FormData.Page, PageSize = FormData.PageSize, TotalResults = ResultCount },
					SortOrder = FormData.SortOrder,
					SortTerm = FormData.SortTerm,
					UserGroup = FormData.UserGroup,
					CrashType = FormData.CrashType,
					SearchQuery = FormData.SearchQuery,
					DateFrom = (long)( FormData.DateFrom - CrashesViewModel.Epoch ).TotalMilliseconds,
					DateTo = (long)( FormData.DateTo - CrashesViewModel.Epoch ).TotalMilliseconds,
					BranchName = FormData.BranchName,
					GameName = FormData.GameName,
					GroupCounts = GroupCounts,
					RealUserName = UniqueUser != null ? UniqueUser.ToString() : null,
				};
			}
		}

		/// <summary>
		/// Gets a container of crash counts per user group for the set of crashes passed in.
		/// </summary>
		/// <param name="EnumerableCrashes">The set of crashes to tabulate by user group.</param>
		/// <returns>A dictionary of user group names, and the count of Buggs for each group.</returns>
		public Dictionary<string, int> GetCountsByGroupFromCrashes( IEnumerable<Crash> EnumerableCrashes )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				Dictionary<string, int> Results = new Dictionary<string, int>();
				var Context = FRepository.Get().Context;

				try
				{
					var UsersIDsAndGroupIDs = Context.Users.Select( User => new { UserId = User.Id, UserGroupId = User.UserGroupId } ).ToList();
					var UserGroupArray = Context.UserGroups.ToList();
					UserGroupArray.Sort( ( UG1, UG2 ) => UG1.Name.CompareTo( UG2.Name ) ); 

					// Initialize all groups to 0.
					foreach( var UserGroup in UserGroupArray )
					{
						Results.Add( UserGroup.Name, 0 );
					}

					Dictionary<int, string> UserIdToGroupName = new Dictionary<int, string>();
					foreach( var UserIds in UsersIDsAndGroupIDs )
					{
						// Find group name for the user id.
						string UserGroupName = UserGroupArray.Where( UG => UG.Id == UserIds.UserGroupId ).First().Name;
						UserIdToGroupName.Add( UserIds.UserId, UserGroupName );
					}

					List<int> UserIdCrashes = EnumerableCrashes.Select( Crash => Crash.UserNameId.Value ).ToList();
					foreach( int UserId in UserIdCrashes )
					{
						string UserGroupName = UserIdToGroupName[UserId];
						Results[UserGroupName]++;
					}
				}
				catch( Exception Ex )
				{
					Debug.WriteLine( "Exception in GetCountsByGroupFromCrashes: " + Ex.ToString() );
				}

				return Results;
			}
		}

		/// <summary>
		/// Filter a set of crashes to a date range.
		/// </summary>
		/// <param name="Results">The unfiltered set of crashes.</param>
		/// <param name="DateFrom">The earliest date to filter by.</param>
		/// <param name="DateTo">The latest date to filter by.</param>
		/// <returns>The set of crashes between the earliest and latest date.</returns>
		public IEnumerable<Crash> FilterByDate( IQueryable<Crash> Results, DateTime DateFrom, DateTime DateTo )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + " SQL" ) )
			{
				IQueryable<Crash> CrashesInTimeFrame = Results
					.Where( MyCrash => MyCrash.TimeOfCrash >= DateFrom && MyCrash.TimeOfCrash <= DateTo.AddDays( 1 ) );
				IEnumerable<Crash> CrashesInTimeFrameEnumerable = CrashesInTimeFrame.ToList();
				return CrashesInTimeFrameEnumerable;
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <typeparam name="TKey"></typeparam>
		/// <param name="Query"></param>
		/// <param name="Predicate"></param>
		/// <param name="bDescending"></param>
		/// <returns></returns>
		public IEnumerable<Crash> EnumerableOrderBy<TKey>( IEnumerable<Crash> Query, Func<Crash, TKey> Predicate, bool bDescending )
		{
			var Ordered = bDescending ? Query.OrderByDescending( Predicate ) : Query.OrderBy( Predicate );
			return Ordered;
		}

		/// <summary>
		/// Apply the sort term to a set of crashes.
		/// </summary>
		/// <param name="Results">The unsorted set of crashes.</param>
		/// <param name="SortTerm">The term to sort by. e.g. UserName</param>
		/// <param name="bSortByDescending">Whether to sort ascending or descending.</param>
		/// <returns>A sorted set of crashes.</returns>
		public IEnumerable<Crash> GetSortedResults( IEnumerable<Crash> Results, string SortTerm, bool bSortByDescending )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(SortTerm=" + SortTerm + ")" ) )
			{
				switch( SortTerm )
				{
					case "Id":
						//Results = bSortByDescending ? Results.OrderByDescending( CrashInstance => CrashInstance.Id ) : Results.OrderBy( CrashInstance => CrashInstance.Id );
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.Id, bSortByDescending );
						break;

					case "TimeOfCrash":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.TimeOfCrash, bSortByDescending );
						break;

					case "UserName":
						// Note: only works where user is stored by Id
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.UserName, bSortByDescending );

							//OrderBy( IntermediateQueryable.Join( Context.Users, CrashInstance => CrashInstance.UserNameId, UserInstance => UserInstance.Id,
							//							( CrashInstance, UserInstance ) => new { CrashInstance, UserInstance.UserName } ),
							//							Joined => Joined.UserName, bSortByDescending ).Select( Joined => Joined.CrashInstance ).AsEnumerable();
						break;

					case "RawCallStack":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.RawCallStack, bSortByDescending );
						break;

					case "GameName":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.GameName, bSortByDescending );
						break;

					case "EngineMode":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.EngineMode, bSortByDescending );
						break;

					case "FixedChangeList":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.FixedChangeList, bSortByDescending );
						break;

					case "TTPID":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.TTPID, bSortByDescending );
						break;

					case "Branch":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.Branch, bSortByDescending );
						break;

					case "ChangeListVersion":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.ChangeListVersion, bSortByDescending );
						break;

					case "ComputerName":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.ComputerName, bSortByDescending );
						break;

					case "PlatformName":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.PlatformName, bSortByDescending );
						break;

					case "Status":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.Status, bSortByDescending );
						break;

					case "Module":
						Results = EnumerableOrderBy( Results, CrashInstance => CrashInstance.Module, bSortByDescending );
						break;
				}

				return Results;
			}
		}

		/// <summary>
		/// Retrieve the parsed callstack for the crash.
		/// </summary>
		/// <param name="CrashInstance">The crash we require the callstack for.</param>
		/// <returns>A class containing the parsed callstack.</returns>
		public CallStackContainer GetCallStack( Crash CrashInstance )
		{
			CachedDataService CachedResults = new CachedDataService( HttpContext.Current.Cache );
			return CachedResults.GetCallStack( CrashInstance );
		}	

		/// <summary> 
		/// This string is written to the report in FCrashReportClient's constructor.
		/// This is obsolete, but left here for the backward compatibility.
		/// </summary>
		const string UserNamePrefix = "!Name:";

		/// <summary> Special user name, currently used to mark crashes from UE4 releases. </summary>
		const string UserNameAnonymous = "Anonymous";

		/// <summary>
		/// Add a new crash to the db based on a CrashDescription sent from the client.
		/// </summary>
		/// <param name="NewCrashInfo">The crash description sent from the client.</param>
		/// <returns>The id of the newly added crash.</returns>
		public int AddNewCrash( CrashDescription NewCrashInfo )
		{
			int NewID = -1;

			Crash NewCrash = new Crash();
			NewCrash.Branch = NewCrashInfo.BranchName;
			NewCrash.BaseDir = NewCrashInfo.BaseDir;
			NewCrash.BuildVersion = NewCrashInfo.BuildVersion;
			NewCrash.ChangeListVersion = NewCrashInfo.BuiltFromCL.ToString();
			NewCrash.CommandLine = NewCrashInfo.CommandLine;
			NewCrash.EngineMode = NewCrashInfo.EngineMode;
		    NewCrash.ComputerName = NewCrashInfo.MachineGuid;

			// Valid MachineID and UserName, updated crash from non-UE4 release
			if(!string.IsNullOrEmpty(NewCrashInfo.UserName))
			{
				NewCrash.UserNameId = FRepository.Get().FindOrAddUser( NewCrashInfo.UserName );
			}
			// Valid MachineID and EpicAccountId, updated crash from UE4 release
			else if(!string.IsNullOrEmpty( NewCrashInfo.EpicAccountId ))
			{
				NewCrash.EpicAccountId = NewCrashInfo.EpicAccountId;
				NewCrash.UserNameId = FRepository.Get().FindOrAddUser( UserNameAnonymous );
			}
			// Crash from an older version.
			else
			{
				// MachineGuid for older crashes is obsolete, so ignore it.
				//NewCrash.ComputerName = NewCrashInfo.MachineGuid;
				NewCrash.UserNameId = FRepository.Get().FindOrAddUser
				(
					!string.IsNullOrEmpty( NewCrashInfo.UserName ) ? NewCrashInfo.UserName : UserNameAnonymous
				);
			}
			
			NewCrash.Description = "";
			if( NewCrashInfo.UserDescription != null )
			{
				NewCrash.Description = string.Join( Environment.NewLine, NewCrashInfo.UserDescription );
			}

			NewCrash.EngineMode = NewCrashInfo.EngineMode;
			NewCrash.GameName = NewCrashInfo.GameName;
			NewCrash.LanguageExt = NewCrashInfo.Language; // Converted by the crash process.
			NewCrash.PlatformName = NewCrashInfo.Platform;

			if( NewCrashInfo.ErrorMessage != null )
			{
				NewCrash.Summary = string.Join( "\n", NewCrashInfo.ErrorMessage );
			}
			
			if (NewCrashInfo.CallStack != null)
			{
				NewCrash.RawCallStack = string.Join( "\n", NewCrashInfo.CallStack );
			}

			if( NewCrashInfo.SourceContext != null )
			{
				NewCrash.SourceContext = string.Join( "\n", NewCrashInfo.SourceContext );
			}

			NewCrash.TimeOfCrash = NewCrashInfo.TimeofCrash;
					
			NewCrash.HasLogFile = NewCrashInfo.bHasLog;
			NewCrash.HasMiniDumpFile = NewCrashInfo.bHasMiniDump;
			NewCrash.HasDiagnosticsFile = NewCrashInfo.bHasDiags;
			NewCrash.HasVideoFile = NewCrashInfo.bHasVideo;
			NewCrash.HasMetaData = NewCrashInfo.bHasWERData;

			NewCrash.bAllowToBeContacted = NewCrashInfo.bAllowToBeContacted;

			NewCrash.TTPID = "";
			NewCrash.FixedChangeList = "";

			// Set the crash type
			NewCrash.CrashType = 1;
			if (NewCrash.RawCallStack != null)
			{
				if (NewCrash.RawCallStack.Contains("FDebug::AssertFailed()"))
				{
					NewCrash.CrashType = 2;
				}
				else if (NewCrash.RawCallStack.Contains("FDebug::EnsureFailed()"))
				{
					NewCrash.CrashType = 3;
				}
			}

			// As we're adding it, the status is always new
			NewCrash.Status = "New";

			/*
				Unused Crashes' fields.
	 
				Title				nchar(20)		
				Selected			bit				
				Version				int				
				AutoReporterID		int				
				Processed			bit	-> renamed to AllowToBeContacted			
				HasDiagnosticsFile	bit	always true		
				HasNewLogFile		bit				
				HasMetaData			bit	always true
			*/
			// Set the unused fields to the default values.
			//NewCrash.Title = "";			removed from dbml
			//NewCrash.Selected = false;	removed from dbml
			//NewCrash.Version = 4;			removed from dbml
			//NewCrash.AutoReporterID = 0;	removed from dbml
			//NewCrash.HasNewLogFile = false;removed from dbml
			// 
			//NewCrash.HasDiagnosticsFile = true;		
			//NewCrash.HasMetaData = true;

			// Add the crash to the database
			var Context = FRepository.Get().Context;
			Context.Crashes.InsertOnSubmit( NewCrash );
			Context.SubmitChanges();

			NewID = NewCrash.Id;

			// Build a callstack pattern for crash bucketing
			NewCrash.BuildPattern();

			return NewID;
		}
	}
}
