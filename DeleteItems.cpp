/**
 * \file DeleteItems.cpp
 * \brief
 * This utility: 
	 1) Deletes the datasets (cancels the check-out)
	 2) Removes from Parent BVR
	 3) Removes the Item from Folder
	 4) Then deletes the Item
 *
 
 * \date $Date: 2016/07/03
 */

#include <tc/tc.h>
#include <base_utils/Mem.h>
#include <pom/pom/pom.h>
#include <tccore/aom.h>
#include <tccore/aom_prop.h>
#include <tc/emh.h>
#include <itk/te.h>
#include <tc/folder.h>
#include <tccore/workspaceobject.h>
#include <sa/user.h>
#include <tccore/grm.h>
#include <pom/enq/enq.h>
#include <res/res_itk.h>

#include <iostream>
#include <fstream>
#include <vector>
using namespace std;

ofstream logFile;

#define ITK(x)																					\
{																								\
	if(iFail == ITK_ok)																			\
	{																							\
		if( (iFail = (x)) != ITK_ok )															\
		{																						\
			char *_szErrString = NULL;															\
			EMH_ask_error_text(iFail, &_szErrString);											\
			logFile << "+++ Error " << iFail << ":" << _szErrString << endl;					\
			logFile << "Function " << #x << "Line " << __LINE__ << "in " << __FILE__<< endl;	\
			if(_szErrString != NULL) MEM_free(_szErrString);									\
		}																						\
	}																							\
}

void showHelp()
{
	cout << "DeleteItems {-u=}{-p=}{-g=} {-cfg=} [-datasetsonly] [-pso] [-nodryrun] [-h]" << endl;
	cout << "-------------------------------------------------------------" << endl;
	cout << "-u=                 User Id" << endl;
	cout << "-p=                 Password" << endl;
	cout << "-g=                 Group" << endl;
	cout << "-cfg=               Input File. [item_id= ]" << endl;
	cout << "-datasetsonly       Delete only the datasets attached with Specification relation" << endl;
	cout << "-pso                Delete the PSOccurrence" << endl;
	cout << "-h                  Help" << endl;
	cout << "-------------------------------------------------------------" << endl;
	cout << "First delete the Datasets only using -datasetsonly, then delete the Assembly" << endl;
	cout << "DeleteItems {-u=}{-p=}{-g=} -cfg=input.txt -datasetsonly -nodryrun (Delete all Datasets first)" << endl;
	cout << "DeleteItems {-u=}{-p=}{-g=} -cfg=input.txt -pso -nodryrun (Remove from parent BOM, then delete the Item)" << endl;
}


int getAllSecondaryDatasets(tag_t tPrimary, int *piSecObjs, tag_t **pptSecObjs)
{
	int iFail = ITK_ok;
	static tag_t __tSecObj = NULLTAG;
	const char *szEnqID = "__GD_Find_SecObjs";

	/* Output initialization */
	*pptSecObjs = NULL;
	*piSecObjs = 0;

	if(__tSecObj == NULLTAG)
	{
		const char *szpSelectAttrList[] = {"puid"};

		/* Create an enquiry */
		ITK(POM_enquiry_create(szEnqID));
		ITK(POM_enquiry_set_distinct(szEnqID, true));

		/* Add Select attributes in the respective classes */
		ITK(POM_enquiry_add_select_attrs(szEnqID, "Dataset", 1, szpSelectAttrList));

		/* Bind Constant values */
		ITK(POM_enquiry_set_tag_value(szEnqID, "primary_tag_val", 1, &tPrimary, POM_enquiry_bind_value));

		/* Join Statements */
		ITK(POM_enquiry_set_attr_expr(szEnqID, "Exp1", "ImanRelation", "primary_object", POM_enquiry_equal, "primary_tag_val"));
		ITK(POM_enquiry_set_join_expr(szEnqID, "Exp2", "ImanRelation", "secondary_object", POM_enquiry_equal, "Dataset", "puid"));

		/* Combined Expressions */
		ITK(POM_enquiry_set_expr(szEnqID, "combined_exp1", "Exp1", POM_enquiry_and, "Exp2"));

		/* Set Where clause */
		ITK(POM_enquiry_set_where_expr(szEnqID, "combined_exp1"));

		if(iFail == ITK_ok)
		{
			/* Cache for the session */
			POM_cache_for_session(&__tSecObj);
			__tSecObj = (tag_t)1;
		}
		else
		{
			ITK(POM_enquiry_delete(szEnqID));
			__tSecObj = NULLTAG;
		}
	}
	else
	{
		/* Bind values */
		ITK(POM_enquiry_set_tag_value(szEnqID, "primary_tag_val", 1, &tPrimary, POM_enquiry_bind_value));
	}

	if((iFail == ITK_ok) && (__tSecObj != NULLTAG))
	{
		int iRows = 0;
		int iCols = 0;
		void ***ppvReport = NULL;

		/* Execute Query */
		ITK(POM_enquiry_execute(szEnqID, &iRows, &iCols, &ppvReport));

		if((iFail == ITK_ok) & (iRows > 0))
		{
			*piSecObjs = iRows;
			*pptSecObjs = (tag_t *) MEM_alloc(sizeof(tag_t*) * iRows);

			for(int k = 0; k < iRows; k++)
			{
				(*pptSecObjs)[k] = *((tag_t *) ppvReport[k][0]);
			}
		}
		MEM_free(ppvReport);
	}
	return (iFail);

}


int getPSOccurrence(tag_t tItem, int *piPSOccurence, tag_t **pptPSOccurence)
{
	int iFail = ITK_ok;
	static tag_t __tPSOccurenceQuery = NULLTAG;
	const char *szEnqID = "__GD_Find_PSOccurence";

	/* Output initialization */
	*pptPSOccurence = NULL;
	*piPSOccurence = 0;

	if(__tPSOccurenceQuery == NULLTAG)
	{
		const char *szpSelectAttrList[] = {"puid"};

		/* Create an enquiry */
		ITK(POM_enquiry_create(szEnqID));
		ITK(POM_enquiry_set_distinct(szEnqID, true));

		/* Add Select attributes in the respective classes */
		ITK(POM_enquiry_add_select_attrs(szEnqID, "PSOccurrence", 1, szpSelectAttrList));

		/* Bind Constant values */
		ITK(POM_enquiry_set_tag_value(szEnqID, "obj_tag_val", 1, &tItem, POM_enquiry_bind_value));

		/* Join Statements */
		ITK(POM_enquiry_set_attr_expr(szEnqID, "Exp", "PSOccurrence", "child_item", POM_enquiry_equal, "obj_tag_val"));

		/* Set Where clause */
		ITK(POM_enquiry_set_where_expr(szEnqID, "Exp"));

		if(iFail == ITK_ok)
		{
			/* Cache for the session */
			POM_cache_for_session(&__tPSOccurenceQuery);
			__tPSOccurenceQuery = (tag_t)1;
		}
		else
		{
			ITK(POM_enquiry_delete(szEnqID));
			__tPSOccurenceQuery = NULLTAG;
		}
	}
	else
	{
		/* Bind values */
		ITK(POM_enquiry_set_tag_value(szEnqID, "obj_tag_val", 1, &tItem, POM_enquiry_bind_value));
	}

	if((iFail == ITK_ok) && (__tPSOccurenceQuery != NULLTAG))
	{
		int iRows = 0;
		int iCols = 0;
		void ***ppvReport = NULL;

		/* Execute Query */
		ITK(POM_enquiry_execute(szEnqID, &iRows, &iCols, &ppvReport));

		if((iFail == ITK_ok) & (iRows > 0))
		{
			*piPSOccurence = iRows;
			*pptPSOccurence = (tag_t *) MEM_alloc(sizeof(tag_t*) * iRows);

			for(int k = 0; k < iRows; k++)
			{
				(*pptPSOccurence)[k] = *((tag_t *) ppvReport[k][0]);
			}
		}
		MEM_free(ppvReport);
	}
	return (iFail);
}


int removeFromParentBOM(tag_t tItem)
{
	int iFail = ITK_ok;
	int iPSOccurence = 0;
	tag_t *ptPSOccurence = NULL;

	ITK(getPSOccurrence(tItem, &iPSOccurence, &ptPSOccurence));

	if ((iFail == ITK_ok) && (iPSOccurence > 0))
	{
		ITK(POM_refresh_instances_any_class(iPSOccurence, ptPSOccurence, POM_modify_lock));
		ITK(POM_delete_instances(iPSOccurence, ptPSOccurence));
	}

	MEM_free(ptPSOccurence);
	return (iFail);
}


int getParetFolders(tag_t tObj, int *piFolder, tag_t **pptFolders)
{
	int iFail = ITK_ok;
	static tag_t __tParentFLQuery = NULLTAG;
	const char *szEnqID = "__GD_Find_Parent_Folder";

	/* Output initialization */
	*pptFolders = NULLTAG;
	*piFolder = 0;

	if(__tParentFLQuery == NULLTAG)
	{
		const char *szpSelectAttrList[] = {"puid"};

		/* Create an enquiry */
		ITK(POM_enquiry_create(szEnqID));
		ITK(POM_enquiry_set_distinct(szEnqID, true));

		/* Add Select attributes in the respective classes */
		ITK(POM_enquiry_add_select_attrs(szEnqID, "Folder", 1, szpSelectAttrList));

		/* Bind Constant values */
		ITK(POM_enquiry_set_tag_value(szEnqID, "obj_tag_val", 1, &tObj, POM_enquiry_bind_value));

		/* Join Statements */
		ITK(POM_enquiry_set_attr_expr(szEnqID, "Exp", "Folder", "contents", POM_enquiry_equal, "obj_tag_val"));

		/* Set Where clause */
		ITK(POM_enquiry_set_where_expr(szEnqID, "Exp"));

		if(iFail == ITK_ok)
		{
			/* Cache for the session */
			POM_cache_for_session(&__tParentFLQuery);
			__tParentFLQuery = (tag_t)1;
		}
		else
		{
			ITK(POM_enquiry_delete(szEnqID));
			__tParentFLQuery = NULLTAG;
		}
	}
	else
	{
		/* Bind values */
		ITK(POM_enquiry_set_tag_value(szEnqID, "obj_tag_val", 1, &tObj, POM_enquiry_bind_value));
	}

	if((iFail == ITK_ok) && (__tParentFLQuery != NULLTAG))
	{
		int iRows = 0;
		int iCols = 0;
		void ***ppvReport = NULL;

		/* Execute Query */
		ITK(POM_enquiry_execute(szEnqID, &iRows, &iCols, &ppvReport));

		if((iFail == ITK_ok) & (iRows > 0))
		{
			*piFolder = iRows;
			*pptFolders = (tag_t *) MEM_alloc(sizeof(tag_t*) * iRows);

			for(int k = 0; k < iRows; k++)
			{
				(*pptFolders)[k] = *((tag_t *) ppvReport[k][0]);
			}
		}
		MEM_free(ppvReport);
	}
	return (iFail);
}


int removeFromParentFolders(tag_t tItem)
{
	int iFail = ITK_ok;
	int iFolders = 0;
	tag_t *ptFolder = NULL;

	ITK(getParetFolders(tItem, &iFolders, &ptFolder));

	if ((iFail == ITK_ok) && (iFolders > 0))
	{
		for (int j = 0 ; j < iFolders ; j++)
		{
			ITK(AOM_lock(ptFolder[j]));
			ITK(FL_remove(ptFolder[j], tItem));
			ITK(AOM_save(ptFolder[j]));
			ITK(AOM_unlock(ptFolder[j]));
		}
	}

	MEM_free(ptFolder);
	return (iFail);
}


int deleteItems(char *szCfg, logical lDatasetsOnly, logical lPSOccurence, logical lNoDryRun)
{
	int iFail = ITK_ok;

	ITK(TE_init_module());
	ITK(TE_ui_set_configfile(szCfg));

	ofstream ouputFile;
	ouputFile.open ("deleteItems_Output.txt");

	ofstream dsOuputFile;
	dsOuputFile.open ("deleteItemsDs_Output.txt");

	{
		int iItemIds = 0;
		char **pszItemIds = NULL;
		char *szItemType = NULL;
		char *szUser = NULL;
		tag_t tItemType = NULLTAG;
		tag_t tUser = NULLTAG;

		ITK(TE_get_preference_list("item_id=", &iItemIds, &pszItemIds));
		cout << "Count of item_id = " << iItemIds << endl << endl;

		cout << "Delete Started" << endl;
		if(lDatasetsOnly == true)
		{
			cout << "\t[Datasets Only]"<< endl;
		}
		if(lNoDryRun == false)
		{
			cout << "\t[Dry Run]"<< endl;
		}
		cout << endl << endl;

		if((iFail == ITK_ok) && (iItemIds > 0) && (pszItemIds != NULL))
		{
			for(int idx = 0; (iFail == ITK_ok) && (idx < iItemIds); idx++)
			{
				int iFoundItems = 0;
				tag_t tItem = NULLTAG;
				tag_t *ptFoundItems = NULL;

				ouputFile << pszItemIds[idx];

				ITK(ITEM_find_in_idcontext(pszItemIds[idx], NULLTAG, &iFoundItems, &ptFoundItems));

				if((iFail == ITK_ok) && (iFoundItems == 1))
				{
					tItem = ptFoundItems[0];
				}

				if(tItem != NULLTAG)
				{
					char *szType = NULL;
					char *szOwner = NULL;
					tag_t tOwner = NULLTAG;
					logical lShouldProcess = true;
					char *szItemID = NULL;

					ITK(ITEM_ask_id2(tItem, &szItemID));

					ITK(ITEM_ask_type2(tItem, &szType));
					ITK(AOM_ask_owner(tItem, &tOwner));
					ITK(SA_ask_user_identifier2(tOwner, &szOwner));

					ouputFile << ";" << szItemID << ";" << szType << ";" << szOwner;

					if(lShouldProcess == true)
					{
						int iMark = 0;
						tag_t tRev = NULLTAG;
						logical lStateChanged = false;

						if(lNoDryRun == true)
						{
							ITK(POM_place_markpoint(&iMark));
						}

						ITK(ITEM_ask_latest_rev(tItem, &tRev));

						if((iFail == ITK_ok) && (tRev != NULLTAG))
						{
							int iDatasets = 0;
							tag_t *ptDatasets = NULL;

							ITK(getAllSecondaryDatasets(tRev, &iDatasets, &ptDatasets));

							if(iDatasets > 0)
							{
								for(int k = 0; k < iDatasets; k++)
								{
									char *dsName = NULL;
									char *dsType = NULL;

									ITK(WSOM_ask_name2(ptDatasets[k], &dsName));
									ITK(WSOM_ask_object_type2(ptDatasets[k], &dsType));

									dsOuputFile << dsName << ";" << dsType << ";";

									if(lNoDryRun == true)
									{
										logical bIsReserved = false;

										ITK(RES_is_checked_out(ptDatasets[k], &bIsReserved));

										if(bIsReserved)
										{
											ITK(RES_cancel_checkout(ptDatasets[k], false));
										}


										ITK(AOM_lock_for_delete(ptDatasets[k]));

										if(iFail == ITK_ok)
										{
											ITK(AOM_delete_from_parent(ptDatasets[k], tRev));

											if(iFail == ITK_ok)
											{
												dsOuputFile << ";Deleted";
											}
										}
										if(iFail != ITK_ok)
										{
											dsOuputFile << ";Error";
										}
									}

									dsOuputFile << endl;

									MEM_free(dsName);
									MEM_free(dsType);
									dsOuputFile.flush();
								}
							}

							MEM_free(ptDatasets);
						}
						if((iFail == ITK_ok) && (lDatasetsOnly == false) && (lNoDryRun == true))
						{
							ITK(removeFromParentFolders(tItem));
							{
								int iRevs = 0;
								tag_t *ptRevs = NULL;

								ITK(ITEM_find_revisions(tItem, "*", &iRevs, &ptRevs));
								for(int idk = 0; idk < iRevs; idk++)
								{
									ITK(removeFromParentFolders(ptRevs[idk]));
								}
							}


							if(lPSOccurence == true)
							{
								ITK(removeFromParentBOM(tItem));
							}

							if (iFail == ITK_ok)
							{
								ITK(AOM_lock_for_delete(tItem));
								if(iFail == ITK_ok)
								{
									ITK(ITEM_delete_item (tItem));
									if(iFail == ITK_ok)
									{
										ouputFile << ";Deleted";
									}
									else
									{
										ouputFile << ";Error";
									}
								}
							}
							else
							{
								ouputFile << ";Error";
							}
						}
						if((lNoDryRun == true) && (iFail != ITK_ok))
						{
							ITK(POM_roll_to_markpoint(iMark, &lStateChanged));
							ouputFile << ";RolledBack";
						}
					}
					else
					{
						ouputFile << ";Skipped:Not_matched";
					}

					MEM_free(szItemID);
					MEM_free(szOwner);
					MEM_free(szType);
				}
				else
				{
					ouputFile << ";;;Not_Found";
				}

				MEM_free(ptFoundItems);

				ouputFile << endl;

				ouputFile.flush();
				dsOuputFile.flush();

				iFail = ITK_ok;
			}
		}

		cout << "Delete Finished."<< endl;

		MEM_free(szItemType);
		MEM_free(szUser);
		MEM_free(pszItemIds);
	}

	ouputFile.close();
	dsOuputFile.close();

	return (iFail);
}

int ITK_user_main(int argc, char* argv[])
{
	int iFail = ITK_ok;
	char *szHelp = NULL;
	//int status=0;
	logFile.open ("deleteItems_log.txt");

	szHelp = ITK_ask_cli_argument("-h");
	if(szHelp == NULL)
	{
		szHelp = ITK_ask_cli_argument("-help");
	}

	if(szHelp != NULL)
	{
		showHelp();
		return (iFail);
	}
	else
	{
		const char *szUserId = NULL;
		char *szPassword = NULL;
		char *szGroup = NULL;
		char *szCfg = NULL;
		char *szNoDryRun = NULL;
		char *szDatasetsOnly = NULL;
		char *szPSO = NULL;
		logical lNoDryRun = false;
		logical lDatasetsOnly = false;
		logical lPSOccurence = false;
		// ITK_initialize_text_services( 0 );
  //  status = ITK_auto_login();
		szUserId = ITK_ask_cli_argument("-u=");
		szPassword = ITK_ask_cli_argument("-p=");
		szGroup = ITK_ask_cli_argument("-g=");
		szCfg = ITK_ask_cli_argument("-cfg=");

		szDatasetsOnly = ITK_ask_cli_argument("-datasetsonly");
		if(szDatasetsOnly != NULL)
		{
			lDatasetsOnly = true;
		}

		szPSO = ITK_ask_cli_argument("-pso");
		if(szPSO != NULL)
		{
			lPSOccurence = true;
		}

		szNoDryRun = ITK_ask_cli_argument("-nodryrun");
		if(szNoDryRun != NULL)
		{
			lNoDryRun = true;
		}

		if((szUserId == NULL) || (szPassword == NULL) || (szGroup == NULL) ||
			(szCfg == NULL))
		{
			cout << endl << "Mandatory parameters are missing: " << endl;
			showHelp();
			return (iFail);
		}

		ITK(ITK_init_module(szUserId, szPassword, szGroup));
		ITK(ITK_set_bypass(true));
		ITK(POM_set_env_info(POM_bypass_attr_update, false, 0, 0, NULLTAG, NULL));

		if (iFail != ITK_ok)
		{
			char *szMessage = NULLTAG;
			EMH_ask_error_text(iFail, &szMessage);
			cout << endl << "##ERROR## Failed to login to Teamcenter: " << iFail << ", " << szMessage << endl;
			showHelp();
			MEM_free(szMessage);
			return iFail;
		}
		if (iFail == ITK_ok)
		{
			cout << "----------------------------------------------------------------------" << endl;
			cout << endl << "Successfully logged in to Teamcenter" << endl;
			cout << "ITK_set_bypass" << endl;

			/*********************************************************************************/

			if(iFail == ITK_ok)
			{
				ITK(deleteItems(szCfg, lDatasetsOnly, lPSOccurence, lNoDryRun));
			}

			/*********************************************************************************/
		}
	}

	ITK_exit_module(TRUE);
	logFile.close();

	return (iFail);
}