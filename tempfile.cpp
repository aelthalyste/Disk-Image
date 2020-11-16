struct int_list{
    int *arr;
    int len;
    int capacity;
};


void 
initialize_int_list(int_list *List, int Capacity){
    
    // NULL
    if(List == NULL){
        return;
    }
    
    List->capacity = Capacity;
    List->len = 0;
    List->arr = new int[Capacity]; // adress doner
    memset(List->arr, 0, sizeof(int)*Capacity);
    
    // your stuff, initialize List accordingly, list ptr MUST have at least Capacity*sizeof(int) memory allocated to itself
}

/*
 new          = delete
malloc       = free
VirtualAlloc = VirtualFree
*/

void 
insert_to_list(int_list *list, int NewElement){
    
    if(list == NULL) return;
    
    if(list->len == list->capacity){
        
        list->capacity = list->capacity * 2;
        
        int *temp_memory = new int[list->capacity];
        memcpy(temp_memory, list->arr, sizeof(int) * (list->capacity));
        
        delete list->arr;
        list->arr = temp_memory;
        
    }
    
    list->arr[list->len] = NewElement;
    list->len++;
    
    // insert element NewElement to list, if list is full, increase its size by reallocating memory
    // careful, after each insertion, len must increase accordingly
}

void 
delete_list(int_list *List){
    
    if(List->arr == NULL) return;
    
    delete List;
    List->len = 0;
    List->capacity =0;
    
    // delete memory allocated to List->ptr COMPLETELY
    // free list that you allocated @initialize_int_list
}


void
UsageTest(int *OutNewLen){
    
    int_list  new int_list; // = {0};
    initialize_int_list(&list, 5);
    
    for(){
        
        // ekleme
    }
    
    insert_to_list(&list, 0);
    insert_to_list(&list, 1);
    insert_to_list(&list, 2);
    insert_to_list(&list, 3);
    insert_to_list(&list, 4);
    insert_to_list(&list, 5);
    insert_to_list(&list, 6);
    insert_to_list(&list, 7);
    insert_to_list(&list, 8);
    
    std::cout<<list.capacity<<"\t"<<list.size<<"\n";
    for(int i = 0; list.len; i++){
        std::cout<<list.ptr[i]<<"\n";
    }
    delete_list(&list);
    
    list.arr;
    
    *OutNewLen = 5;
    
}

void foo(){
    
}



/*
    ASSUMES RECORDS ARE SORTED
THIS FUNCTION REALLOCATES MEMORY VIA realloc(), DO NOT PUT MEMORY OTHER THAN ALLOCATED BY MALLOC, OTHERWISE IT WILL CRASH THE PROGRAM
*/
void
MergeRegions(data_array<nar_record>* R) {
    
    UINT32 MergedRecordsIndex = 0;
    UINT32 CurrentIter = 0;
    
    for (;;) {
        if (CurrentIter >= R->Count) {
            break;
        }
        
        UINT32 EndPointTemp = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
        
        if (IsRegionsCollide(R->Data[MergedRecordsIndex], R->Data[CurrentIter])) {
            UINT32 EP1 = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
            UINT32 EP2 = R->Data[MergedRecordsIndex].StartPos + R->Data[MergedRecordsIndex].Len;
            
            EndPointTemp = MAX(EP1, EP2);
            R->Data[MergedRecordsIndex].Len = EndPointTemp - R->Data[MergedRecordsIndex].StartPos;
            
            CurrentIter++;
        }
        else {
            MergedRecordsIndex++;
            R->Data[MergedRecordsIndex] = R->Data[CurrentIter];
        }
        
        
    }
    
    R->Count = MergedRecordsIndex + 1;
    R->Data = (nar_record*)realloc(R->Data, sizeof(nar_record) * R->Count);
    
}



int main(){
    
    // NOTE(Batuhan): bunun uzunlugunun 1 oldugunun dikkatini cekerim xD
    int arr[1];
    
    int len;
    std::cin>>len;
    
    for(int i =0; i<len; i++){
        arr[i] = factorial[i];
    } 
    
    return 0;
}