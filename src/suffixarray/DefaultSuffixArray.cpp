#include "DefaultSuffixArray.h"
#include "DefaultSuffixArrayFactory.h"

#include "../sais/sais.h"

namespace PgSAIndex {

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::DefaultSuffixArray(PseudoGenome* pseudoGenome)
    : // up to 2 additional bytes to avoid overflowing during casting to uint_reads_cnt
    SuffixArrayBase(pseudoGenome->getLength(), getSuffixArraySizeInBytesWithGuard(pseudoGenome), pseudoGenome),
    OccurrencesIterator(
    *ReadsListIteratorFactoryTemplate<ReadsListClass>::getReadsListIterator(* ((ReadsListClass*) pseudoGenome->getReadsList()))),
    pseudoGenome(pseudoGenome),
    readsList(pseudoGenome->getReadsList()),
    suffixArray(0),
    lookupTable(lookupTableKeyPrefixLength, pseudoGenome->getReadsSetProperties()) {
        
        cout << "SA creation start.\n";
        
//        if ((sizeof(uint_pg_len) == sizeof(int)) && (pseudoGenome->getLengthWithGuard() < INT_MAX / sizeof(int)))
//            generateSaisPgSA();
//        else
            generatePgSA();
        
        this->lookupTable.generate(this, this->getElementsCount());
        //                cout << "memcheck 4.....\n";
        //                cin.ignore(1);
        buildReadsWithDuplicatesFilter();
        //                cout << "memcheck 5.....\n";
        //                cin.ignore(1);
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::DefaultSuffixArray(PseudoGenome* pseudoGenome, std::istream& src)
    : SuffixArrayBase(pseudoGenome->getLength(), getSuffixArraySizeInBytesWithGuard(pseudoGenome), pseudoGenome),
    OccurrencesIterator(
    *ReadsListIteratorFactoryTemplate<ReadsListClass>::getReadsListIterator(* ((ReadsListClass*) pseudoGenome->getReadsList()))),
    pseudoGenome(pseudoGenome),
    readsList(pseudoGenome->getReadsList()),
    lookupTable(lookupTableKeyPrefixLength, pseudoGenome->getReadsSetProperties()) {
        uint_max arraySize;
        src >> arraySize;
        src.get(); // '/n'

        if (arraySize != this->getSizeInBytes())
            cout << "WARNING: wrong size of suffixarray.";

        suffixArray = (uchar*) PgSAHelpers::readArray(src, this->getSizeInBytes() * sizeof (uchar));
        src.get(); // '/n'

        this->lookupTable.read(src);

    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::prepareUnsortedSA() {
        suffixArray = new uchar[this->getSizeInBytes()];
        
        //TODO: check if setting 0 is necessary.... 
        suffixArray[this->getSizeInBytes() - 2] = 0;
        suffixArray[this->getSizeInBytes() - 1] = 0;

        //               cout << "memcheck 3.....\n";
        //                cin.ignore(1);
        
        const uchar* curSAPos = suffixArray;

        uint_reads_cnt readsListIndex = 0;

        for (uint_pg_len pgPos = 0; pgPos < this->elementsCount; pgPos++) {
            while (readsList->getReadPosition(readsListIndex + 1) <= pgPos)
                readsListIndex++;
            *((uint_reads_cnt*) curSAPos) = readsListIndex;
            *((uint_read_len*) (curSAPos + POS_START_OFFSET)) = pgPos - readsList->getReadPosition(readsListIndex);
            curSAPos += SA_ELEMENT_SIZE;
        }

        if (curSAPos != suffixArray + this->pseudoGenome->getLength() * (uint_max) SA_ELEMENT_SIZE)
            cout << "WARNING: SA generation failed: " << (int) (curSAPos - suffixArray) / SA_ELEMENT_SIZE << " elements instead of " << this->pseudoGenome->getLength() << "\n";

        pgStatic = this->pseudoGenome;
        maxReadLength = this->pseudoGenome->maxReadLength();
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::generateSaisPgSA() {
        clock_checkpoint();
        
        int* saisSA = (int*) malloc((size_t)(this->pseudoGenome->getLengthWithGuard()) * sizeof(int));

        if(sais((const unsigned char*) this->pseudoGenome->getSuffix(0), saisSA, (int) this->pseudoGenome->getLengthWithGuard()) != 0) {
            fprintf(stderr, "Cannot allocate memory.\n");
            exit(EXIT_FAILURE);
        }
  
        std::ofstream dest("saisSA.tmp", std::ios::out | std::ios::binary);
        PgSAHelpers::writeArray(dest, saisSA, (size_t)(this->pseudoGenome->getLengthWithGuard()) * sizeof(int));
        dest.close();
        free((void*)saisSA);
        
        cout << "SAIS generation time " << clock_millis() << " msec!\n";
        
        clock_checkpoint();
        
        prepareUnsortedSA();
        
        std::ifstream src("saisSA.tmp", std::ifstream::binary);
        const uchar* curSAPos = suffixArray;
        int pgPos;
        for (uint_pg_len i = 0; i < this->elementsCount;) {
            src.read((char*) &pgPos, sizeof(int));
            if ((uint_pg_len) pgPos < this->elementsCount) {
                while ((uint_pg_len) pgPos < i)
                    pgPos = this->getSuffixByAddress(saPosIdx2Address(pgPos)) - this->pseudoGenome->getSuffix(0);
                
                swapElementsByAddress((sa_pos_addr*) curSAPos, saPosIdx2Address(pgPos));
                curSAPos += SA_ELEMENT_SIZE;
                i++;
            }
        }
        
        cout << "SA generation time " << clock_millis() << " msec!\n";
    }
    
    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::generatePgSA() {
        clock_checkpoint();

        prepareUnsortedSA();
        
        qsort(suffixArray, this->elementsCount, sizeof (uchar) * SA_ELEMENT_SIZE, this->pgSuffixesCompare);

        cout << "SA generation time " << clock_millis() << " msec!\n";
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    int DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::pgSuffixesCompare(const void* a, const void* b) {
        const char_pg* readA = getSuffixStatic((sa_pos_addr) a);
        const char_pg* readB = getSuffixStatic((sa_pos_addr) b);

        int i = 0;
        while (i++ < maxReadLength) {
            if (*readA > *readB)
                return 1;
            if (*readA++ < *readB++)
                return -1;
        }
        return 0;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const char_pg* DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getSuffixStatic(const sa_pos_addr& saPosAddress) {
        return pgStatic->getSuffix(getReadsListIndexByAddress(saPosAddress), getPosStartOffsetByAddress(saPosAddress));
    }
    
    
    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::buildReadsWithDuplicatesFilter() {
        clock_checkpoint();

        readsList->setDuplicateFilterKmerLength(this->lookupTable.getKeyPrefixLength());
        if (readsList->getDuplicateFilterKmerLength() != this->lookupTable.getKeyPrefixLength()) {
            cout << "Unsupported duplicate filter size " << readsList->getDuplicateFilterKmerLength() << " expected " << this->lookupTable.getKeyPrefixLength() << "!\n";
            exit(-1);
        }

        uint_max filterCount = 0;
        uint_max lutSize = lookupTable.getLookupTableLengthWithGuard();

        for (uint_max j = 0; j < lutSize - 1; j++)
            filterCount += markReadsWithDuplicates(j);

        cout << "Found " << filterCount << " reads containing duplicate " << (int) readsList->getDuplicateFilterKmerLength()
                << "-mers in " << clock_millis() << " msec!\n";
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    uint_max DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::markReadsWithDuplicates(uint_max lutIdx) {

        readsIdxs.clear();
        uint_pg_len start = lookupTable.getRawValue(lutIdx);
        uint_pg_len stop = lookupTable.getRawValue(lutIdx + 1);
        const uint_read_len guardOffset = readsList->getMaxReadLength() - readsList->getDuplicateFilterKmerLength();

        for (uint_pg_len i = start; i < stop; i++) {
            const sa_pos_addr saPosAddress = this->saPosIdx2Address(i);
            uint_reads_cnt j = this->getReadsListIndexByAddress(saPosAddress);
            uint_pg_len guard = this->getPosStartOffsetByAddress(saPosAddress) + readsList->getReadPosition(j) - guardOffset;
            while (readsList->getReadPosition(j) >= guard) {
                if (!readsList->hasOccurFlag(j))
                    readsIdxs.push_back(j);
                readsList->setOccurOnceFlag(j);
                if (j == 0) break;
                j--;
            }
        }

        uint_reads_cnt j = 0;
        uint_reads_cnt readsCount = readsIdxs.size();
        for (uint_reads_cnt i = 0; i < readsCount; i++) {
            if (!readsList->hasOccurOnceFlag(readsIdxs[i]) && !readsList->hasDuplicateFilterFlag(readsIdxs[i])) {
                readsList->setDuplicateFilterFlag(readsIdxs[i]);
                j++;               
            } else
                readsList->clearOccurFlags(readsIdxs[i]);
        }

        return j;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::write(ostream& dest) {
        dest << this->getSizeInBytes() << "\n";
        PgSAHelpers::writeArray(dest, suffixArray, this->getSizeInBytes());
        dest << "\n";
        lookupTable.write(dest);
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::~DefaultSuffixArray() {
        delete[] (this->suffixArray);
        delete(pseudoGenome);
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    typename DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::int_kmers_comp DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::kmerSAComp(const char* kmerPtr, const uint_read_len& kmerLength, const uint_pg_len& suffixIdx, const uint_read_len& lcp) {
        kmerPtr += lcp;
        const char* suffixPtr = getSuffixByAddress(saPosIdx2Address(suffixIdx)) + lcp;

        uint_read_len i = lcp;

        while (kmerLength - i >= 2) {
            int cmp = bswap_16(*(uint16_t*) kmerPtr) - bswap_16(*(uint16_t*) suffixPtr);
            if (cmp != 0)
                break;
            kmerPtr += 2;
            suffixPtr += 2;
            i += 2;
        }

        while (i < kmerLength) {
            i++;
            int cmp = *kmerPtr++ - *suffixPtr++;
            if (cmp > 0)
                return i;
            if (cmp < 0)
                return -i;
        }

        return 0;

    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::kmerRangeBSearch(const char* kmerPtr, const uint_read_len& kmerLength, SARange<uint_pg_len>& range) {

        uint_pg_len mIdx;
        int_kmers_comp cmpRes;

        uint_read_len lcp = lookupTableKeyPrefixLength;

        uint_read_len lcp_l = lcp;
        uint_read_len lcp_r = lcp;
        uint_read_len lcp_beg = lcp;
        uint_read_len lcp_end = lcp;

        // lower_bound search
        uint_pg_len lIdx = range.start;
        uint_pg_len rIdx = range.end;

        while (lIdx < rIdx) {
            mIdx = (lIdx + rIdx) / 2;
            cmpRes = kmerSAComp(kmerPtr, kmerLength, mIdx, lcp);

            if (cmpRes > 0) {
                range.start = mIdx + 1;
                lIdx = mIdx + 1;

                lcp = ((lcp_beg = lcp_l = cmpRes - 1) <= lcp_r) ? lcp_l : lcp_r;
            } else if (cmpRes < 0) {
                range.end = mIdx;
                rIdx = mIdx - 1;

                lcp = ((lcp_end = lcp_r = -cmpRes - 1) <= lcp_l) ? lcp_r : lcp_l;
            } else {
                if (range.start < mIdx)
                    range.start = mIdx + 1;
                rIdx = mIdx;

                lcp_beg = lcp_r = kmerLength;
            }
        }

        if (lIdx == rIdx && lIdx == range.start) {
            // start might be smaller then kmer
            cmpRes = kmerSAComp(kmerPtr, kmerLength, lIdx, lcp);

            if (cmpRes > 0) {
                range.start = ++lIdx;
                lcp_beg = lcp_l;
            } else if (cmpRes < 0) {
                range.end = lIdx;
                lcp_end = lcp_l;
            } else {
                if (range.start < lIdx)
                    range.start = lIdx + 1;
                lcp_beg = lcp_l;
            }
        }

        uint_pg_len start = lIdx;

        // upper_bound search
        lIdx = range.start;
        rIdx = range.end;
        lcp_l = lcp_beg;
        lcp_r = lcp_end;
        lcp = lcp_l < lcp_r ? lcp_l : lcp_r;

        while (lIdx < rIdx) {
            mIdx = (lIdx + rIdx) / 2;
            cmpRes = kmerSAComp(kmerPtr, kmerLength, mIdx, lcp);

            if (cmpRes > 0) {
                lIdx = mIdx + 1;

                lcp = ((lcp_l = cmpRes - 1) <= lcp_r) ? lcp_l : lcp_r;
            } else if (cmpRes < 0) {
                rIdx = mIdx;

                lcp = ((lcp_r = -cmpRes - 1) <= lcp_l) ? lcp_r : lcp_l;
            } else {
                lIdx = mIdx + 1;

                lcp_l = kmerLength;
            }
        }

        range = {start, lIdx};
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const string DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getDescriptionImpl() {
        string desc;
        uint_max size = sizeof (this) + this->getSizeInBytes() +
                sizeof (this->lookupTable) + this->lookupTable.getLookupTableLengthWithGuard() * sizeof (uint_pg_len) +
                sizeof (this->pseudoGenome) + this->pseudoGenome->getLengthWithGuard() * sizeof (char_pg) +
                sizeof (this->readsList) + (uint_max) this->readsList->getReadsCount() * this->readsList->getListElementSize();
        desc = desc + "Standard SA\t TOTAL (Pg+RL+SA+LT) " + toMB(size, 2) + " MB\n"
                + "Pg: " + toMB(this->pseudoGenome->getLengthWithGuard() * sizeof (char_pg), 2) + " MB\t"
                + "RL: " + toMB((uint_max) this->readsList->getReadsCount() * this->readsList->getListElementSize(), 2) + " MB\t"
                + "SA: " + toMB(this->getSizeInBytes(), 2) + " MB\t"
                + "LT: " + toMB(this->lookupTable.getLookupTableLengthWithGuard() * sizeof (uint_pg_len), 2) + " MB\n";
        desc = desc + toString(this->readsList->getReadsCount()) + " ";
        if (this->pseudoGenome->isReadLengthConstant())
            desc = desc + "constant";
        else
            desc = desc + "variable";
        desc = desc + " length reads\t max length: " + toString(this->pseudoGenome->maxReadLength())
                + "\t Pg length: " + toString(this->pseudoGenome->getLength()) + "\n";
        return desc;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::findKmerRangeImpl(const char_pg* kmer, const uint_read_len& kmerLength, SARange<uint_pg_len>& range) {
        if (!lookupTable.findSARange(kmer, kmerLength, range))
            kmerRangeBSearch(kmer, kmerLength, range);
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::findOccurrencesOfImpl(const char_pg* kmer, const uint_read_len kmerLength) {
        this->findKmerRange(kmer, kmerLength, range);
        this->readsIterator.initIteration(kmerLength);

        if (range.start != range.end)
            this->readsIterator.setIterationPosition(this->getPosition(range.start));
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    typename DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::OccurrencesIterator& DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getKmerOccurrencesIteratorImpl(const string& kmer) {
        this->findOccurrencesOf(kmer.data(), kmer.length());
        return *this;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    typename DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::OccurrencesIterator& DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getKmerOccurrencesIteratorImpl(const uint_reads_cnt originalIdx, const uint_read_len pos, const uint_read_len kmerLength) {
        const char_pg* kmer = this->pseudoGenome->getSuffixPtrByPosition(originalIdx, pos);
        this->findOccurrencesOf(kmer, kmerLength);
        return *this;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    bool DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::moveNextImpl() {
        if (this->readsIterator.moveNext())
            return true;

        while (++(range.start) < range.end) {
            this->readsIterator.setIterationPosition(this->getPosition(range.start));
            if (this->readsIterator.moveNext())
                return true;
        }

        return false;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const RPGOffset<uint_read_len, uint_reads_cnt> DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getPositionImpl(const uint_pg_len posIdx) {
        return getPositionByAddress(saPosIdx2Address(posIdx));
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const RPGOffset<uint_read_len, uint_reads_cnt> DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getPositionByAddress(const sa_pos_addr& saPosAddress) {
        return
        {
            getReadsListIndexByAddress(saPosAddress), getPosStartOffsetByAddress(saPosAddress)
        };
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const uint_read_len DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getPosStartOffsetByAddress(const sa_pos_addr saPosAddress) {
        return *((uint_read_len*) ((uchar*) saPosAddress + POS_START_OFFSET));
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const uint_reads_cnt DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getReadsListIndexByAddress(const sa_pos_addr saPosAddress) {
        return (*((uint_reads_cnt*) (saPosAddress))) & READSLIST_INDEX_MASK;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    ReadsSetInterface<uint_read_len, uint_reads_cnt>* DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getReadsSetImpl() {
        return this->pseudoGenome;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    uint_max DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getSuffixArraySizeInBytesWithGuard(PseudoGenome* pseudoGenome) {
        return sizeof (uchar) * ((pseudoGenome->getLength()) + 2) * SA_ELEMENT_SIZE;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const char_pg* DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getSuffixByAddress(const sa_pos_addr& saPosAddress) {
        return this->pseudoGenome->getSuffix(getReadsListIndexByAddress(saPosAddress), getPosStartOffsetByAddress(saPosAddress));
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const string DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getSuffixImpl(const uint_pg_len posIdx, const uint_pg_len length) {
        return string(getSuffixByAddress(saPosIdx2Address(posIdx)), length);
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    string DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::getTypeID() {
        return PGSATYPE_DEFAULT;
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    const sa_pos_addr DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::saPosIdx2Address(const uint_pg_len posIdx) {
        return (sa_pos_addr) (suffixArray + ((uint_max) posIdx) * SA_ELEMENT_SIZE);
    }

    template<typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len, uchar SA_ELEMENT_SIZE, uchar POS_START_OFFSET, uint_reads_cnt READSLIST_INDEX_MASK, class ReadsListClass>
    void DefaultSuffixArray<uint_read_len, uint_reads_cnt, uint_pg_len, SA_ELEMENT_SIZE, POS_START_OFFSET, READSLIST_INDEX_MASK, ReadsListClass>::swapElementsByAddress(const sa_pos_addr saPosAddressFst, const sa_pos_addr saPosAddressSnd) {
        uchar tmp;
        for(int i = 0; i < SA_ELEMENT_SIZE; i++) {
            tmp = *(((uchar*) saPosAddressFst)+i);
            *(((uchar*) saPosAddressFst)+i) = *(((uchar*) saPosAddressSnd)+i);
            *(((uchar*) saPosAddressSnd)+i) = tmp;
        }
    }

    template class DefaultSuffixArray<uint_read_len_min, uint_reads_cnt_std, uint_pg_len_std, 4, 3, 0x00FFFFFF, typename ListOfConstantLengthReadsTypeTemplate<uint_read_len_min, uint_reads_cnt_std, uint_pg_len_std>::Type>;
    template class DefaultSuffixArray<uint_read_len_min, uint_reads_cnt_std, uint_pg_len_max, 4, 3, 0x00FFFFFF, typename ListOfConstantLengthReadsTypeTemplate<uint_read_len_min, uint_reads_cnt_std, uint_pg_len_max>::Type>;
    template class DefaultSuffixArray<uint_read_len_min, uint_reads_cnt_std, uint_pg_len_std, 5, 4, 0xFFFFFFFF, typename ListOfConstantLengthReadsTypeTemplate<uint_read_len_min, uint_reads_cnt_std, uint_pg_len_std>::Type>;
    template class DefaultSuffixArray<uint_read_len_min, uint_reads_cnt_std, uint_pg_len_max, 5, 4, 0xFFFFFFFF, typename ListOfConstantLengthReadsTypeTemplate<uint_read_len_min, uint_reads_cnt_std, uint_pg_len_max>::Type>;
    template class DefaultSuffixArray<uint_read_len_std, uint_reads_cnt_std, uint_pg_len_std, 5, 3, 0x00FFFFFF, typename ListOfConstantLengthReadsTypeTemplate<uint_read_len_std, uint_reads_cnt_std, uint_pg_len_std>::Type>;
    template class DefaultSuffixArray<uint_read_len_std, uint_reads_cnt_std, uint_pg_len_max, 5, 3, 0x00FFFFFF, typename ListOfConstantLengthReadsTypeTemplate<uint_read_len_std, uint_reads_cnt_std, uint_pg_len_max>::Type>;
    template class DefaultSuffixArray<uint_read_len_std, uint_reads_cnt_std, uint_pg_len_std, 6, 4, 0xFFFFFFFF, typename ListOfConstantLengthReadsTypeTemplate<uint_read_len_std, uint_reads_cnt_std, uint_pg_len_std>::Type>;
    template class DefaultSuffixArray<uint_read_len_std, uint_reads_cnt_std, uint_pg_len_max, 6, 4, 0xFFFFFFFF, typename ListOfConstantLengthReadsTypeTemplate<uint_read_len_std, uint_reads_cnt_std, uint_pg_len_max>::Type>;

}