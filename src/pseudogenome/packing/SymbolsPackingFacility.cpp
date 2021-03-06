#include "SymbolsPackingFacility.h"

namespace PgSAIndex {

    template<typename uint_element>
    SymbolsPackingFacility<uint_element>::SymbolsPackingFacility(ReadsSetProperties* readsSetProperties, uchar symbolsPerElement)
    : symbolsCount(readsSetProperties->symbolsCount),
    symbolsPerElement(symbolsPerElement) {
        uint_max combinationCount = powuint(symbolsCount, symbolsPerElement);
        maxValue = combinationCount - 1;
        if (maxValue > (int) (uint_element) - 1)
            cout << "ERROR in symbols packaging: max value for type: " << (int) (uint_element) - 1 << " while max " << " \n";

        reverse = new char_pg*[combinationCount];
        reverseFlat = new char_pg[combinationCount * symbolsPerElement];

        clear = new uint_element*[combinationCount];
        clearFlat = new uint_element[combinationCount * symbolsPerElement];

        std::copy(std::begin(readsSetProperties->symbolsList), std::end(readsSetProperties->symbolsList), std::begin(symbolsList));
        std::copy(std::begin(readsSetProperties->symbolOrder), std::end(readsSetProperties->symbolOrder), std::begin(symbolOrder));
        for (uint_max i = 0; i < UCHAR_MAX; i++)
            if (symbolOrder[i] == -1)
                symbolOrder[i] = 0;

        buildReverseAndClearIndexes();
    }

    template<typename uint_element>
    SymbolsPackingFacility<uint_element>::~SymbolsPackingFacility() {
        delete[]reverse;
        delete[]reverseFlat;
        delete[]clear;
        delete[]clearFlat;
    }

    template<typename uint_element>
    void SymbolsPackingFacility<uint_element>::buildReverseAndClearIndexes() {
        char_pg* rPtr = reverseFlat;
        uint_element* cPtr = clearFlat;
        for (uint_max i = 0; i <= maxValue; i++) {
            reverse[i] = rPtr;
            rPtr += symbolsPerElement;
            clear[i] = cPtr;
            cPtr += symbolsPerElement;
        }

        uint_element* currentClear = new uint_element[symbolsPerElement]();
        uint_symbols_cnt* sequence = new uint_symbols_cnt[symbolsPerElement]();
        for (uint_max i = 0; i <= maxValue; i++) {
            for (uchar j = 0; j < symbolsPerElement; j++) {
                reverse[i][j] = symbolsList[sequence[j]];
                clear[i][j] = currentClear[j];
            }

            uchar j = symbolsPerElement - 1;
            while ((++sequence[j] == symbolsCount) && ((int) j > 0)) {
                sequence[j] = 0;
                currentClear[j] = i + 1;
                j--;
            }
        }
    }

    template<typename uint_element>
    uint_element SymbolsPackingFacility<uint_element>::clearSuffix(const uint_element value, uchar prefixLength) {
        return clear[value][prefixLength];
    }

    template<typename uint_element>
    uint_element SymbolsPackingFacility<uint_element>::getMaxValue() {
        return maxValue;
    }

    template<typename uint_element>
    bool SymbolsPackingFacility<uint_element>::isCompatibile(uchar symbolsPerElement, uchar symbolsCount) {
        return powuint(symbolsCount, symbolsPerElement) - 1 <= (uint_element) - 1;
    }

    template<typename uint_element>
    uchar SymbolsPackingFacility<uint_element>::maxSymbolsPerElement(uchar symbolsCount) {
        for (int i = 0; i < UCHAR_MAX; i++)
            if (!SymbolsPackingFacility<uint_element>::isCompatibile(i + 1, symbolsCount))
                return i;
        return UCHAR_MAX;
    }

    template<typename uint_element>
    uint_element SymbolsPackingFacility<uint_element>::packPrefixSymbols(const char_pg* symbols, const uint_max length) {
        uint_element value = 0;
        for (uchar j = 0; j < length; j++)
            value = value * symbolsCount + symbolOrder[(uchar) symbols[j]];

        return value;
    }

    template<typename uint_element>
    uint_max SymbolsPackingFacility<uint_element>::packSequence(const char_pg* source, const uint_max length, uint_element* dest) {
        uint_max i = 0;

        const char_pg* guard = source + length - symbolsPerElement;

        while (source <= guard) {
            dest[i++] = packSymbols(source);
            source += symbolsPerElement;
        }

        uint_max left = guard + symbolsPerElement - source;
        if (left > 0)
            dest[i++] = packSuffixSymbols(source, left);

        return i;
    }

    template<typename uint_element>
    uint_element SymbolsPackingFacility<uint_element>::packSuffixSymbols(const char_pg* symbols, const uint_max length) {
        uint_element value = 0;
        for (uchar j = 0; j < symbolsPerElement; j++) {
            value *= symbolsCount;
            if (j < length)
                value += symbolOrder[(uchar) symbols[j]];
        }
        return value;
    }

    template<typename uint_element>
    uint_element SymbolsPackingFacility<uint_element>::packSymbols(const char_pg* symbols) {
        uint_element value = 0;
        for (uchar j = 0; j < symbolsPerElement; j++)
            value = value * symbolsCount + symbolOrder[(uchar) symbols[j]];

        return value;
    }

    template<typename uint_element>
    inline const string SymbolsPackingFacility<uint_element>::reverseSequence(const uint_element* sequence, const uint_max pos, const uint_max length) {
        string res;
        uint_max i = divideBySmallInteger(pos, symbolsPerElement);
        uint_max reminder = moduloBySmallInteger(pos, this->symbolsPerElement, i);
        res.append(reverseValue(sequence[i++]), reminder, this->symbolsPerElement - reminder);
        while (res.size() < length)
            res.append(reverseValue(sequence[i++]));
        res.resize(length);
        return res;
    }

    template<typename uint_element>
    inline void SymbolsPackingFacility<uint_element>::reverseSequence(const uint_element* sequence, const uint_max pos, const uint_max length, char_pg* destPtr) {

        uint_max i = divideBySmallInteger(pos, symbolsPerElement);
        uint_max reminder = moduloBySmallInteger(pos, this->symbolsPerElement, i);

        char_pg* ptr = destPtr;
        while (true) {
            uint_element value = sequence[i++];
            for (uchar j = reminder; j < symbolsPerElement; j++) {
                *ptr++ = reverse[value][j];
                if ((uint_max) (ptr - destPtr) == length)
                    return;
            }
            reminder = 0;
        }
   }

    template<typename uint_element>
    inline char_pg SymbolsPackingFacility<uint_element>::reverseValue(uint_element value, uchar position) {
        return reverse[value][position];
    }

    template<typename uint_element>
    inline string SymbolsPackingFacility<uint_element>::reverseValue(uint_element value) {
        string res;
        res.resize(symbolsPerElement);
        for (uchar j = 0; j < symbolsPerElement; j++)
            res[j] = reverse[value][j];
        return res;
    }

    template<typename uint_element>
    inline int SymbolsPackingFacility<uint_element>::compareSequences(uint_element* lSeq, uint_element* rSeq, const uint_max length) {
        uint_max i = length;
        while (i >= symbolsPerElement) {
            if (*lSeq > *rSeq)
                return 1;
            if (*lSeq++ < *rSeq++)
                return -1;
            i -= symbolsPerElement;
        } 
        
        int j = 0;
        while (i--) {
            if (reverse[*lSeq][j] > reverse[*rSeq][j])
                return 1;
            if (reverse[*lSeq][j] < reverse[*rSeq][j])
                return -1;
            j++;
        }
        
        return 0;
    }

    template<typename uint_element>
    inline int SymbolsPackingFacility<uint_element>::compareSequences(uint_element* lSeq, uint_element* rSeq, uint_max pos, uint_max length) {
        uint_max i = divideBySmallInteger(pos, symbolsPerElement);
        uint_max reminder = moduloBySmallInteger(pos, this->symbolsPerElement, i);

        lSeq += i;
        rSeq += i;
        for (uchar j = reminder; j < symbolsPerElement; j++) {
            if (reverse[*lSeq][j] > reverse[*rSeq][j])
                return 1;
            if (reverse[*lSeq][j] < reverse[*rSeq][j])
                return -1;
            if (--length == 0)
                return 0;
        }
        
        return compareSequences(lSeq + 1, rSeq + 1, length);
    }

    template<typename uint_element>
    inline int SymbolsPackingFacility<uint_element>::compareSuffixWithPrefix(uint_element* sufSeq, uint_element* preSeq, uint_max sufPos, uint_max length) {
        uint_max i = divideBySmallInteger(sufPos, symbolsPerElement);
        uint_max reminder = moduloBySmallInteger(sufPos, this->symbolsPerElement, i);
        
        sufSeq += i;
        if (reminder == 0)
            return compareSequences(sufSeq, preSeq, length);
        
        uchar sufIdx = reminder;
        uchar preIdx = 0;
        while (true) {
            if (reverse[*sufSeq][sufIdx] > reverse[*preSeq][preIdx])
                return 1;
            if (reverse[*sufSeq][sufIdx] < reverse[*preSeq][preIdx])
                return -1;
            if (--length == 0)
                return 0;
            if (++preIdx == symbolsPerElement) {
                preIdx = 0; preSeq++;
            } 
            if (++sufIdx == symbolsPerElement) {
                sufIdx = 0; sufSeq++;
            }
        }
    }
    
    template class SymbolsPackingFacility<uint_ps_element_min>;
    template class SymbolsPackingFacility<uint_ps_element_std>;
}