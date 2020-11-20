#include <stdio.h>
#define STACK_SIZE 256

struct stack{
    linked_list* Data[256];
    int Index;
};

struct linked_list{
    linked_list *Next;
    int Data;
};

linked_list*
CreateHead(int Val){
    linked_list *Result = (linked_list*)malloc(sizeof(linked_list));
    Result->Next = 0;
    Result->Data = Val;
    return (Result);
}

void Insert(linked_list *Head, int Val){
    linked_list *Temp = Head;
    while(Temp->Next) Temp = Temp->Next;
    Temp->Next = (linked_list*)malloc(sizeof(linked_list));
    Temp->Next->Next = 0;
    Temp->Data = Val;
}

void PrintNumbers(linked_list *L){
    linked_list* T = L;
    printf("ID -> %i\t Number->",T->Data);
    T = T->Next;
    while(T){
        printf("%i",T->Data);
        T = T->Data;
    }
    printf("\n");
}

linked_list*
PopStack(stack* Stack){
    void* Result = Stack->Data[(Index--) - 1];
    return Result;
}

void
PushStack(stack* Stack, linked_list *Adress){
    Stack->Data[Index++] = Adress;
}


int main(){
    int SmallestID = 1000;// Since max ID can't exceed 999, it's safe to use 1000 as min so it will be rounded down to first ID encountered
    stack Stack = {{0},0};
    
    for(;;){
        char Input[12];
        scanf("%s",Input);
        if(Input[0] == 'q'){
            break; //quit and print list
        }
        
        int ID = 0;
        ID += (Input[11] - '0');
        ID += (Input[10] - '0')*10;
        ID += (Input[9] - '0')*100;
        linked_list* L = CreateHead(ID);
        for(int i =0; i<12;i++){
            Insert(L,(Input[i] - '0'));
        }
        PushStack(&Stack,L);
        printf("ID %i\n");
        if(ID<SmallestID) SmallestID = ID;
        
    }
    
    for(int i =0;i<Stack.Index; i++){
        PrintNumbers(Stack.Data[i]);
    }
    printf("\nSmallest number = %i\n",SmallestID);
    
}
