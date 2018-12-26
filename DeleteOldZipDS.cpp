/**
 * \file DeleteDS.cpp
 * \brief
 * This utility: 
 *
 * Except the Latest Modified Zip-Datasets, it deletes the older Zip datasets (cancels the check-out) 
 *
 * \note
 * \author 
 * \version 
 * \date $Date: 2016/07/12
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
	cout << "DeleteDS {-u=}{-p=}{-g=} {-cfg=} [-nodryrun] [-h]" << endl;
	cout << "-------------------------------------------------------------" << endl;
	cout << "-u=                 User Id" << endl;
	cout << "-p=                 Password" << endl;
	cout << "-g=                 Group" << endl;
	cout << "-cfg=               Input File. [item_id= ]" << endl;
	cout << "-h                  Help" << endl;
	cout << "-------------------------------------------------------------" << endl;
}


int getAllSecondaryZipDatasets(tag_t tPrimary, int *piSecObjs, tag_t **pptSecObjs)
{
	int iFail = ITK_ok;
	static tag_t __tSecObj = NULLTAG;
	const char *szEnqID = "__GD_Find_SecZips";

	/* Output initialization */
	*pptSecObjs = NULL;
	*piSecObjs = 0;

	if(__tSecObj == NULLTAG)
	{
		const char *szpSelectAttrList[] = {"puid", "last_mod_date"};
		const char *szValue = "Zip";

		/* Create an enquiry */
		ITK(POM_enquiry_create(szEnqID));
		ITK(POM_enquiry_set_distinct(szEnqID, true));

		/* Add Select attributes in the respective classes */
		ITK(POM_enquiry_add_select_attrs(szEnqID, "Dataset", 2, szpSelectAttrList));

		/* Bind Constant values */
		ITK(POM_enquiry_set_tag_value(szEnqID, "primary_tag_val", 1, &tPrimary, POM_enquiry_bind_value));
		ITK(POM_enquiry_set_string_value(szEnqID, "InputValueExprId", 1, &szValue, POM_enquiry_const_value));

		/* Join Statements */
		ITK(POM_enquiry_set_attr_expr(szEnqID, "Exp1", "ImanRelation", "primary_object", POM_enquiry_equal, "primary_tag_val"));
		ITK(POM_enquiry_set_join_expr(szEnqID, "Exp2", "ImanRelation", "secondary_object", POM_enquiry_equal, "Dataset", "puid"));
		ITK(POM_enquiry_set_attr_expr(szEnqID, "Exp3", "Dataset", "object_type", POM_enquiry_equal, "InputValueExprId"));

		/* desc_order */
		ITK(POM_enquiry_add_order_attr(szEnqID, "Dataset", "last_mod_date", POM_enquiry_desc_order));

		/* Combined Expressions */
		ITK(POM_enquiry_set_expr(szEnqID, "combined_exp1", "Exp1", POM_enquiry_and, "Exp2"));
		ITK(POM_enquiry_set_expr(szEnqID, "Final_Exp", "combined_exp1", POM_enquiry_and, "Exp3"));

		/* Set Where clause */
		ITK(POM_enquiry_set_where_expr(szEnqID, "Final_Exp"));

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


int deleteDS(char *szCfg, logical lNoDryRun)
{
	int iFail = ITK_ok;

	ITK(TE_init_module());
	ITK(TE_ui_set_configfile(szCfg));

	ofstream ouputFile;
	ouputFile.open ("deleteDS_Output.txt");

	ofstream dsOuputFile;
	dsOuputFile.open ("deleteDSDs_Output.txt");

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
					tag_t tRev = NULLTAG;

					ITK(ITEM_ask_id2(tItem, &szItemID));
					ITK(ITEM_ask_type2(tItem, &szType));
					ITK(AOM_ask_owner(tItem, &tOwner));
					ITK(SA_ask_user_identifier2(tOwner, &szOwner));

					ouputFile << ";" << szItemID << ";" << szType << ";" << szOwner;

					ITK(ITEM_ask_latest_rev(tItem, &tRev));

					if((iFail == ITK_ok) && (tRev != NULLTAG))
					{
						int iZipDatasets = 0;
						int iNonZipDatasets = 0;
						tag_t *ptZipDatasets = NULL;
						tag_t *ptNonZipDatasets = NULL;

						/*************************************************************************/
						// Find All Zip Datasets, but delete except the 1st One
						ITK(getAllSecondaryZipDatasets(tRev, &iZipDatasets, &ptZipDatasets));

						if((iFail == ITK_ok) && (iZipDatasets > 1))
						{
							for(int k = 1; ((iFail == ITK_ok) && (k < iZipDatasets)); k++)
							{
								char *dsName = NULL;
								char *dsType = NULL;

								ITK(WSOM_ask_name2(ptZipDatasets[k], &dsName));
								ITK(WSOM_ask_object_type2(ptZipDatasets[k], &dsType));

								dsOuputFile << dsName << ";" << dsType << ";";

								if(lNoDryRun == true)
								{
									logical bIsReserved = false;

									ITK(RES_is_checked_out(ptZipDatasets[k], &bIsReserved));

									if(bIsReserved)
									{
										ITK(RES_cancel_checkout(ptZipDatasets[k], false));
									}

									ITK(AOM_lock_for_delete(ptZipDatasets[k]));

									if(iFail == ITK_ok)
									{
										ITK(AOM_delete_from_parent(ptZipDatasets[k], tRev));

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
						/*************************************************************************/

						MEM_free(ptZipDatasets);
						MEM_free(ptNonZipDatasets);
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

	logFile.open ("deleteDS_log.txt");

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
		logical lNoDryRun = false;

		szUserId = ITK_ask_cli_argument("-u=");
		szPassword = ITK_ask_cli_argument("-p=");
		szGroup = ITK_ask_cli_argument("-g=");
		szCfg = ITK_ask_cli_argument("-cfg=");

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
				ITK(deleteDS(szCfg, lNoDryRun));
			}
			/*********************************************************************************/
		}
	}

	ITK_exit_module(TRUE);
	logFile.close();

	return (iFail);
}