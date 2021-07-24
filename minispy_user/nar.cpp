#include "nar.h"

/*
setups iter for finding intersections of two regions. 
Assumes both regions are sorted.
*/
inline RegionCoupleIter
NarStartRegionIntersectionIter(nar_record *R1, nar_record *R2, size_t R1Len, size_t R2Len){
    RegionCoupleIter Result = NarInitRegionCoupleIter(R1, R2, R1Len, R2Len);
    NarNextRegionIntersection(&Result);
    return Result;
}

inline void
NarNextRegionIntersection(RegionCoupleIter *Iter){
    
    Iter->It = {};
    if(NarIterateRegionCoupleUntilCollision(Iter)){
        uint32_t R1REnd = Iter->R1Iter->StartPos + Iter->R1Iter->Len;
        uint32_t R2REnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        
        // intersection
        uint32_t IntersectionEnd   = MIN(R1REnd, R2REnd);
        uint32_t IntersectionStart = MAX(Iter->R1Iter->StartPos, Iter->R2Iter->StartPos);
        
        Iter->It = {IntersectionStart, IntersectionEnd - IntersectionStart};
        
        if(R1REnd < R2REnd) Iter->R1Iter++;
        else                Iter->R2Iter++;
    }
    
}


/*
Setups Iter for finding exclusion of Ex from Base.
*/
inline RegionCoupleIter
NarStartExcludeIter(nar_record *Base, nar_record *Ex, size_t BaseN, size_t ExN){
    RegionCoupleIter Result = NarInitRegionCoupleIter(Base, Ex, BaseN, ExN);
    NarNextExcludeIter(&Result);
    return Result;
}


inline void
NarNextExcludeIter(RegionCoupleIter *Iter){
    
    // R1 == base regions
    // R2 == regions to exclude from base
    Iter->It = {};
    
    nar_record Result = {0};
    if(NarIterateRegionCoupleUntilCollision(Iter)){
        ASSERT(Iter->R1Iter < Iter->R1End);
        ASSERT(Iter->R2Iter < Iter->R2End);
        
        uint32_t BEnd = Iter->R1Iter->StartPos + Iter->R1Iter->Len;
        uint32_t EEnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        
        // NOTE(Batuhan): this diagram can't be seen properly in 4coder..
        // open file in notepad or something simple.
        
        /*
        condition 1:
            --------------
        -------                             MUST ITERATE THAT ONE
    
        condition 2:
            -----------------               MUST ITERATE THAT ONE
                        ------------
    
        condition 3:
            ------------------
                --------                    MUST ITERATE THAT ONE
    
        condition 4:
            -----------------
            -----------------
                doesnt really matter which one you iterate, fits in algorithm below
    
        as you can see, if we would like to spot continuous colliding blocks, we must ALWAYS ITERATE THAT HAS LOWER END POINT, and their intersection WILL ALWAYS BE REPRESENTED AS
        HIGH_START - LOW_END of both of them.
    */
        if(EEnd >= Iter->R1Iter->StartPos && Iter->R2Iter->StartPos <= Iter->R1Iter->StartPos){
            Iter->It.StartPos = EEnd;
            Iter->It.Len      = BEnd -EEnd;
            Iter->R2Iter++;
        }
        else if(Iter->R2Iter->StartPos <= BEnd && EEnd >= BEnd){
            Iter->It.StartPos = Iter->R1Iter->StartPos;
            Iter->It.Len      = BEnd - Iter->R2Iter->StartPos;
            Iter->R1Iter++;
        }
        else if(Iter->R2Iter->StartPos > Iter->R1Iter->StartPos && EEnd <= BEnd){
            Iter->It.StartPos = Iter->R1Iter->StartPos;
            Iter->It.Len      = Iter->R2Iter->StartPos - Iter->R1Iter->StartPos;
            Iter->R2Iter++;
        }
        else if(EEnd == BEnd && Iter->R1Iter->StartPos == Iter->R2Iter->StartPos){
            Iter->R2Iter++;
            Iter->R1Iter++;
        }
        else{
            ASSERT(FALSE);
        }
    }
    
    
}

inline RegionCoupleIter
NarInitRegionCoupleIter(nar_record *Base, nar_record *Ex, size_t BaseN, size_t ExN){
    RegionCoupleIter Result = {};
    
    Result.R1     = Base;
    Result.R1Len  = BaseN;
    Result.R1Iter = &Result.R1[0];
    Result.R1End  = Result.R1 + Result.R1Len;
    
    Result.R2     = Ex;
    Result.R2Len  = ExN;
    Result.R2Iter = &Result.R2[0];
    Result.R2End  = Result.R2 + Result.R2Len;
    
    Result.It     = {0, 0}; 
    return Result;
}


// returns true if it finds valid collision block. false is it depletes 
// Iter
inline bool
NarIterateRegionCoupleUntilCollision(RegionCoupleIter *Iter){
    
    for(;;){
        if(Iter->R1Iter == Iter->R1End){
            break;
        }
        if(Iter->R2Iter == Iter->R2End){
            break;
        }
        
        uint32_t R1REnd = Iter->R1Iter->StartPos + Iter->R1Iter->Len;
        uint32_t R2REnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        if(R1REnd < Iter->R2Iter->StartPos){
            Iter->R1Iter++;
            continue;
        }
        if(R2REnd < Iter->R1Iter->StartPos){
            Iter->R2Iter++;
            continue;
        }
        
        break;
    }
    
    return NarIsRegionIterValid(Iter);
}


inline bool
NarIsRegionIterValid(RegionCoupleIter *Iter){
    return !!(Iter->R1Iter == Iter->R1End || Iter->R2Iter == Iter->R2End);
}
