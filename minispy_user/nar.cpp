#include "nar.h"

/*
setups iter for finding intersections of two regions. 
Assumes both regions are sorted.
*/
inline RegionCoupleIter
NarStartIntersectionIter(const nar_record *R1, const nar_record *R2, size_t R1Len, size_t R2Len){
    RegionCoupleIter Result = NarInitRegionCoupleIter(R1, R2, R1Len, R2Len);
    NarNextIntersectionIter(&Result);
    return Result;
}

inline void
NarNextIntersectionIter(RegionCoupleIter *Iter){
    
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
NarStartExcludeIter(const nar_record *Base, const nar_record *Ex, size_t BaseN, size_t ExN){
    RegionCoupleIter Result = NarInitRegionCoupleIter(Base, Ex, BaseN, ExN);
    Result.__CompRegion = *Result.R1Iter;
    NarNextExcludeIter(&Result);
    return Result;
}


inline void
NarNextExcludeIter(RegionCoupleIter *Iter){
    
    // R1 == base regions
    // R2 == regions to exclude from base
    
    Iter->It = {};
    
    while(1){
        
        if(Iter->R1Iter == Iter->R1End){
            return;
        }
        if(Iter->R2Iter == Iter->R2End){
            return;
        }
        
        uint32_t R1REnd = Iter->__CompRegion.StartPos + Iter->__CompRegion.Len;
        uint32_t R2REnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        
        if(R1REnd < Iter->R2Iter->StartPos){
            Iter->It = Iter->__CompRegion;
            Iter->R1Iter++;
            Iter->__CompRegion = (Iter->R1Iter != Iter->R1End) ? *Iter->R1Iter : nar_record{};
            return;
        }
        if(R2REnd < Iter->__CompRegion.StartPos){
            Iter->R2Iter++;
            continue;
        }
        
        break;
    }
    
    
    
    ASSERT(Iter->R1Iter < Iter->R1End);
    ASSERT(Iter->R2Iter < Iter->R2End);
    
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
    
    /*
for from left   : loop till end of collision
for from right  : terminate
for from middle : return left one, make __compregion right one
for overshadow  : loop till end of collision
*/
    
    while(1){
        
        uint32_t BEnd = Iter->__CompRegion.StartPos + Iter->__CompRegion.Len;
        uint32_t EEnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        
        if(Iter->R1Iter == Iter->R1End){
            break;
        }
        if(Iter->R2Iter == Iter->R2End){
            break;
        }
        
        if((Iter->__CompRegion.StartPos < Iter->R2Iter->StartPos && BEnd < Iter->R2Iter->StartPos)
           || (Iter->__CompRegion.StartPos > EEnd))
        {
            // no collision
            break;
        }
        
        // collision from left
        if(Iter->__CompRegion.StartPos >= Iter->R2Iter->StartPos &&
           EEnd >= Iter->R2Iter->StartPos && EEnd <= BEnd){
            Iter->It.StartPos = EEnd;
            Iter->It.Len      = BEnd -EEnd;
            ASSERT(BEnd > EEnd);
            Iter->__CompRegion = {EEnd, BEnd - EEnd};
            Iter->R2Iter++;
        }
        // collision from right
        else if(Iter->R2Iter->StartPos >= Iter->__CompRegion.StartPos && 
                Iter->R2Iter->StartPos <= BEnd && EEnd >= BEnd){
            Iter->It.StartPos = Iter->__CompRegion.StartPos;
            Iter->It.Len      = Iter->R2Iter->StartPos - Iter->__CompRegion.StartPos;
            ASSERT(BEnd > Iter->R2Iter->StartPos);
            Iter->R1Iter++;
            Iter->__CompRegion = (Iter->R1Iter != Iter->R1End) ? *Iter->R1Iter : nar_record{};
            break;
        }
        // collision from middle
        else if(Iter->R2Iter->StartPos > Iter->It.StartPos &&
                EEnd < BEnd){
            Iter->It.StartPos = Iter->__CompRegion.StartPos;
            Iter->It.Len      = Iter->R2Iter->StartPos - Iter->__CompRegion.StartPos;
            ASSERT(Iter->R2Iter->StartPos > Iter->__CompRegion.StartPos);
            Iter->__CompRegion = {EEnd, BEnd - EEnd};
            Iter->R2Iter++;
            break;
        }
        // r2 overshadows r1 completely
        else if(Iter->R2Iter->StartPos <= Iter->__CompRegion.StartPos &&
                EEnd >= BEnd){
            Iter->It.StartPos = Iter->__CompRegion.StartPos;
            Iter->R1Iter++;
            Iter->__CompRegion = Iter->R1Iter != Iter->R1End ? *Iter->R1Iter : nar_record{};
        }
        else{
            break;
        }
        
    }
    
    
}

inline RegionCoupleIter
NarInitRegionCoupleIter(const nar_record *Base, const nar_record *Ex, size_t BaseN, size_t ExN){
    RegionCoupleIter Result = {};
    
    Result.R1     = Base;
    Result.R1Len  = BaseN;
    Result.R1Iter = Result.R1;
    Result.R1End  = Result.R1 + Result.R1Len;
    
    Result.R2     = Ex;
    Result.R2Len  = ExN;
    Result.R2Iter = Result.R2;
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
    
    return !__NarIsRegionIterExpired(*Iter);
}


inline bool
NarIsRegionIterValid(RegionCoupleIter Iter){
    return (Iter.It.Len != 0);
}


inline bool
__NarIsRegionIterExpired(RegionCoupleIter Iter){
    return (Iter.R1Iter == Iter.R1End || Iter.R2Iter == Iter.R2End);
}

