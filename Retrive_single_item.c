

#include <tc/iman.h>
#include <ai/sample_err.h>
#include <pom/pom/pom.h>
#include <tccore/item.h>
#include <tccore/aom.h>
#include <tccore/aom_prop.h>
#include <stdio.h>
#include <stdlib.h>
#include <pom/pom/pom.h>
#include <qry/qry.h>
#include <fclasses/iman_date.h>
#include <cfm/cfm.h>
#include <tccore/imantype.h>

static logical listRelationTypes(void) {
    int i;
    int relationTypes;
    
    tag_t type;
    tag_t *relationTypeList;
    
    char typeName[IMANTYPE_name_size_c + 1];
    
    CALL(GRM_list_relation_types(&relationTypes, &relationTypeList));
    
    printf("Number of Relation Types: %d\n", relationTypes);
    
    for(i = 0; i < relationTypes; i++) {
        CALL(IMANTYPE_ask_name(relationTypeList[i], typeName));
        printf("\t%s\n", typeName);
    }
    MEM_free(relationTypeList);
    
}

int getItemRevisionStatus( tag_t checkItemRevision, int* isReleased ) {
    logical is_displayable = TRUE;
    
    int property_count = 0;
    int ii;
    int n_statuses;
    
    tag_t *statuses;
    
    char  **property_names = NULL;
    char itemRevisionName_[ITEM_name_size_c + 1] = "";
    
    // Get all the property names and property count
    CALL( AOM_ask_prop_names(checkItemRevision, &property_count, &property_names) );
    
    // Reset value
    *isReleased = 0;
    
    
    for (ii = 0; ii < property_count; ii++) {
        // Only process the "release_stauses" list
        if (!strcmp(property_names[ii], "release_statuses")) {
            // Check how many properties are "release_statuses"
            CALL( AOM_ask_value_tags(checkItemRevision, property_names[ii], &n_statuses, &statuses) );
            // If 1 or more release statuses are found return release status true
            if (n_statuses > 0) {
                *isReleased = 1;
                if (n_statuses) MEM_free(statuses);
            }
        }
    }
    
    MEM_free(property_names);
    
    return 0;
}


int NXDatasetsExists(tag_t itemRevision, int* hasNXDatasets) {
    int numberOfObjects = 0;
    int ii = 0;
    
    tag_t relation_type = NULLTAG;
    tag_t classOfFile = NULLTAG;
    tag_t *objects;
    
    char *className = NULL;
    char type[WSO_name_size_c+1] = "";
    
    CALL( GRM_find_relation_type("IMAN_specification", &relation_type) );
    
    if (relation_type != NULLTAG) {
        CALL( GRM_list_secondary_objects_only(itemRevision, relation_type, &numberOfObjects, &objects) );
    }
    
    if (numberOfObjects > 0) {
        for (ii = 0; ii < numberOfObjects; ii++){
            CALL( POM_class_of_instance(objects[ii], &classOfFile) );
            CALL( POM_name_of_class(classOfFile, &className) );
            if (strcmp(className, "Dataset") == 0) {
                CALL( WSOM_ask_object_type(objects[ii], type) );
                // If there is a UGMaster return 1
                if (strcmp(type, "UGMASTER") == 0) {
                    *hasNXDatasets = 1;
                }
                // If there is a UGPart return 1
                if (strcmp(type, "UGPART") == 0) {
                    *hasNXDatasets = 1;
                }
            }
        }
        if (className) MEM_free(className);
    }
    else { // No UGMaster or UGParts found, return 0
        *hasNXDatasets = 0;
    }
    
    if (objects) MEM_free(objects);
    
    return 0;
}


int getItemRevisions(char *outputFileName, char *itemID) {
    
    char *select_attrs[2] = {"item_id", "puid"};
    char *vals[1] = {"Item"};
    char itemRevisionName[ITEM_name_size_c + 1] = "";
    char itemRevisionID[ITEM_id_size_c + 1] = "";
    char itemType[ITEM_type_size_c+1] = "";
    
    tag_t tag_uom = NULLTAG;
    tag_t *revisions = NULL;
    tag_t  item ;
    // tag_t  items = NULLTAG;

    int columns = 0;
    int ii;
    int irows = 0;
    int itemRevisionCount = 0;
    int rev = 0;
    int isReleased = 0;
    int isNextReleased = 0;
    int hasNXDatasets = 0;
    
    FILE *out_file;
    char *tag_string;
    
   
    //char* ITEM_ID;

	const char *ITEM_ID="000018";
    // Find Item Tag
    CALL(   ITEM_find_item(ITEM_ID,&item) );
    // TODO: Check full NULLTAG
    
    // Get Item Type
   CALL( ITEM_ask_type(item, itemType) );
  //CALL( ITEM_delete_item(item));
	    printf("Working \n");
    // Open or Create a file in Appended mode
    out_file=fopen(outputFileName, "a");
    
    fprintf(out_file, "    <item>\n");
    
    fprintf(out_file, "      <itemType>");
    fprintf(out_file, "%s", itemType);
    fprintf(out_file, "</itemType>\n");
    
    fprintf(out_file, "      <id>");
    fprintf(out_file, "%s", ITEM_ID);
    fprintf(out_file, "</id>\n");
    
    fprintf(out_file, "      <name></name>\n");
    
    
    // Find all the item revisions for the items
    CALL( ITEM_list_all_revs( item, &itemRevisionCount, &revisions) );
    
    // Find all revisions of the found Items
    for (rev = 0; rev < itemRevisionCount; rev++) {
        CALL( ITEM_ask_rev_name( revisions[rev], itemRevisionName ) );
        CALL( ITEM_ask_rev_id( revisions[rev], itemRevisionID ) );
        
        // Check if item revision has a status
        getItemRevisionStatus(revisions[rev], &isReleased);
        
        // Peaking ahead to see next revision is released or not (looking for latest released)
        if ( rev < itemRevisionCount-1 ) {
            getItemRevisionStatus(revisions[rev+1], &isNextReleased);
        }
        else {
            isNextReleased = 0;
        }
        
        
        // Check if the itemRevision has NX Datasets
        NXDatasetsExists(revisions[rev], &hasNXDatasets);
        
        // Only output if there is at least one NX Dataset
        
        fprintf(out_file, "      <itemRevision>\n");
        
        fprintf(out_file, "            <status>");
        
        // If the item revision is released
        if (isReleased == 1 ) {
            // If this is the last revision and it is released (no working revs)
            if ( rev == itemRevisionCount-1 && isNextReleased == 0 ) {
                fprintf(out_file, "Latest Released");
            }
            else if ( isNextReleased == 0 ) { // if next rev is working set this to latest
                fprintf(out_file, "Latest Released");
            }
            else { // if next rev is released set this rev to released
                fprintf(out_file, "Released");
            }
        }
        else {
            fprintf(out_file, "Working");
        }
        
        fprintf(out_file, "</status>\n");
        fprintf(out_file, "            <revision>%s</revision>\n", itemRevisionID);
        fprintf(out_file, "            <description></description>\n");
        
        if (hasNXDatasets > 0 ) {
            fprintf(out_file, "            <NXDataset value=\"True\"/>\n");
        }
        else {
            fprintf(out_file, "            <NXDataset value=\"False\"/>\n");
        }
        
        fprintf(out_file, "      </itemRevision>\n");
        
    } // End Revision loop
    fprintf(out_file, "    </item>\n");
    
    
    fclose(out_file);
   // CALL( ITEM_delete_item(item));
    return 0;
}



int ITK_user_main(int argc, char* argv[]) {
    
    // Get required argument(s) from command line
    char *itemID=ITK_ask_cli_argument("-itemID=");
    char *outputFileName=ITK_ask_cli_argument("-file=");
    int rows = 0;
    int status = 0;
    char *message;
    void ***items = NULL;
        char* ITEM_ID;

    int ii=0;
    char *tag_string;
    	outputFileName = "outputFile.csv";
    if(outputFileName == NULL) { printf("missing required -file=outputFile.csv"); return(0); }

   // if(outputFileName == NULL) { printf("missing required -file=c:\\temp\\outputFile.txt"); return(0); }
   // if(itemID == NULL) { printf("missing required -itemID=123456789"); return(0); }
    
    ITK_initialize_text_services( 0 );
    status = ITK_auto_login();
    
    if ( status != ITK_ok ) {
        printf("Login not successful\n");
    }
    else {
        printf("Login successful\n");
        ITK_set_journalling(FALSE);
        
        //listRelationTypes();
        getItemRevisions(outputFileName,  itemID);
    }
    
    
    ITK_exit_module(TRUE);
    return status;
}
