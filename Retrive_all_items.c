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

static logical listRelationTypes(void)	{
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


int getItems(char *outputFileName, int *rows, char  ***items) {
    
    char *select_attrs[2] = {"item_id", "puid"};
    char *vals[1] = {"Item"};
    char itemRevisionName[ITEM_name_size_c + 1] = "";
    char itemRevisionID[ITEM_id_size_c + 1] = "";
    char itemType[ITEM_type_size_c+1] = "";
    
    tag_t tag_uom = NULLTAG;
    tag_t *revisions = NULL;
        tag_t  item ;

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
  
    int prop_count;
    char  **prop_names;
  
    CALL( POM_enquiry_create("enquiry") );
    CALL( POM_enquiry_add_select_attrs("enquiry", "Item", 2, select_attrs) );
    //CALL(POM_enquiry_set_tag_value("enquiry", "value", 1, &tag_uom, POM_enquiry_bind_value));
    CALL( POM_enquiry_set_string_value("enquiry", "value", 1, vals, POM_enquiry_const_value) );
    CALL( POM_enquiry_set_attr_expr("enquiry", "expression", "Item", "object_type",  POM_enquiry_equal , "value") );
    CALL( POM_enquiry_set_where_expr( "enquiry", "expression")  );
    CALL( POM_enquiry_execute("enquiry", &irows, &columns, &items) );
	
    printf("working....\n");
    
    // Assign the output rows to the # of rows found
    *rows = irows;
    
    
out_file=fopen(outputFileName, "a");

    if (irows == 0) {
                fprintf(out_file, "");

           }   
    
    //If there are any rows found
    if (irows > 0) {
             fprintf(out_file, "itemID, \n\n");
        for (ii = 0; ii < irows; ii++) {
            
            CALL( ITEM_ask_type( *((tag_t *)items[ii][1]) , itemType) );
             
            fprintf(out_file, "%s, \n", ((char *)items[ii][0]));
			
           } // End Revision loop
        
    } // End Item loop

	
    



    fclose(out_file);
    
    return 0;
}






int ITK_user_main(int argc, char* argv[]) {
    
    // Get required argument(s) from command line
    char *itemType=ITK_ask_cli_argument("-itemType=");
    char *outputFileName=ITK_ask_cli_argument("-file=");

   
	 


    int rows = 0;
    int status = 0;
    char *message;
   tag_t  *items = NULL;
        char* ITEM_ID;

   
    int ii=0;
    char *tag_string;
     char *ch;   
       
	    char *tmpID;   
	 char *str_ItemID; 
	 char *is_ItemDelete; 
	
    

	outputFileName = "outputFile.csv";
    if(outputFileName == NULL) { printf("missing required -file=outputFile.csv"); return(0); }
   
    ITK_initialize_text_services( 0 );
    status = ITK_auto_login();
    
    if ( status != ITK_ok ) {
        printf("Login not successful\n");
    }
    else {
        printf("Login successful\n");
        ITK_set_journalling(FALSE);
        
        //listRelationTypes();
        getItems(outputFileName, &rows, items);

		      

        }
    
    ITK_exit_module(TRUE);
    return status;
}