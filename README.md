# ITK-Customization---Teamcenter-
1. Retrieve all items.C:                  
    This ITK code retrieves all the Item details  from the Teamcenter Database.
    
2. Retrieve single item.C:                   
    This ITK code retrieves only single item details from Teamcenter, where the "itemID" should be initialized with the item-id value         which we want to retrieve. 
    
3.Deleteitems.cpp:            
    This ITK code deletes all the items specified in the input file. 
    
4.DeleateOldZips.cpp:                     
    This ITK code deletes the zip files which are oldest among all the attached datasets but not the latest zip file.  

Please make sure that the following inputs are given in the following manner:  
    PROPERTIES-->DEBUGGING-->COMMAND ARGUMENTS-->-u=(the user name) -p=(the password) -g=(the group)  -cfg=(the input file which contains all the item id's in which the zips to be deleted)  -nodryrun. 
    
*NOTE:-If you don't mention "-nodryrun" then there is no deletion in the database but runs simply.   

Please make sure that the following changes are done before compiling: 
    SOLUTION EXPLORER--> right click on the PROJECT--> SELECT PROPERTIES--> In the Properties dialog box, Expand C/C++ --> select PREPROCESSOR --> In the "PREPROCESSOR  DEFINATIONS" add the following   
        _HAS_ITERATOR_DEBUGGING=0;_ITERATOR_DEBUG_LEVEL=0;
