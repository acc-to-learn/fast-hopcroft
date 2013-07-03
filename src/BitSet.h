// June 2013, Jairo Andres Velasco Romero, jairov(at)javerianacali.edu.co
#pragma once

#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <functional>
#include "BitUtil_uint32.h"
#include "BitUtil_uint64.h"

/// Represents a Set. Internal storage uses bit vectors.
/// Every member of the Set is an integer number in a zero-based range.
/// It allows Union, Intersect, Add, Remove, Copy, Complement oprations to run in constant time.
/// <param ref="TElement" /> is the integer type for each state. Note that it affects maximum elements number.
/// <param ref="TToken" /> is the integer type to internal storage.
template<class TElement, class TToken>
class BitSet
{
	static const unsigned int ElementsPerToken = sizeof(TToken)*8;
	typedef BitUtil<TToken, false> BU;

	TToken* TokenArray;
	TToken LastTokenMask;
	unsigned Tokens;
	unsigned MaxElements;
		
	 std::size_t ReqSize(unsigned r) { return sizeof(TToken)*std::max(r, 16u); }
public:

	/// <param ref="maxElements" /> Indicates the maximum number of elements
	BitSet(unsigned maxElements)
	{
		Tokens = maxElements / ElementsPerToken;
		if(Tokens * ElementsPerToken != maxElements) Tokens++;
		TokenArray = (TToken*)malloc(ReqSize(Tokens));
		MaxElements = maxElements;
				
		LastTokenMask = 0;
		auto offset = maxElements % ElementsPerToken;
		for(unsigned i=0; i<offset; i++) BU::SetBit(&LastTokenMask, i);

		Clear();
	}

	/// Copy constructor
	BitSet(const BitSet& copyFrom)
	{
		Tokens = copyFrom.Tokens;
		LastTokenMask = copyFrom.LastTokenMask;
		MaxElements = copyFrom.MaxElements;
		TokenArray = (TToken*)malloc(ReqSize(Tokens));
		CopyFrom(copyFrom);
	}

	/// Move constructor
	BitSet(BitSet&& rhs)
	{
		TokenArray = rhs.TokenArray;
		LastTokenMask = rhs.LastTokenMask;
		Tokens = rhs.Tokens;
		MaxElements = rhs.MaxElements;

		rhs.TokenArray = nullptr;
	}

	/// Destructor
	~BitSet()
	{
		if(TokenArray != nullptr) 
		{ 
			free(TokenArray);
			TokenArray = nullptr;
		}
	}

	/// Assign operator
	BitSet& operator= (const BitSet& rh)
	{
		if(rh.Tokens != Tokens) 
		{
			Tokens = rh.Tokens;
			LastTokenMask = rh.LastTokenMask;
			MaxElements = rh.MaxElements;
			TokenArray = (TToken*)realloc(TokenArray, ReqSize(Tokens));
		}
		CopyFrom(rh);
		return *this;
	}

	/// Assign operator
	BitSet& operator= (const BitSet&& rhs)
	{
		if(rhs != this)
		{
			TokenArray = rhs.TokenArray;
			LastTokenMask = rhs.LastTokenMask;
			MaxElements = rhs.MaxElements;
			Tokens = rhs.Tokens;

			rhs.TokenArray = nullptr;
		}
		return *this;
	}

	/// Removes all members of the set
	void Clear()
	{
		BU::ClearAllBits(TokenArray, Tokens);
	}

	/// Copy other set defined under same universe.
	/// It means that source Set hold the same maximum number of elements.
	/// O(1)
	void CopyFrom(const BitSet& copyFrom)
	{
		assert(copyFrom.Tokens == Tokens);
		assert(copyFrom.LastTokenMask == LastTokenMask);
		assert(copyFrom.MaxElements == MaxElements);
		memcpy(TokenArray, copyFrom.TokenArray, sizeof(TToken)*Tokens);
	}

	/// Check membership of <param ref="element" />
	/// O(1)
	bool Contains(TElement element) const
	{
		assert(element < Tokens*ElementsPerToken);
		assert(element < MaxElements);

		// Optmizer will reduce integer divisions to bit shifts
		auto token = element / ElementsPerToken;
		auto offset = element % ElementsPerToken;

		return BU::TestBit(&TokenArray[token], offset);
	}

	/// Add one element 
	/// O(1)
	void Add(TElement element)
	{
		assert(element < Tokens*ElementsPerToken);
		assert(element < MaxElements);

		// Optmizer will reduce integer divisions to bit shifts
		auto token = element / ElementsPerToken;
		auto offset = element % ElementsPerToken;

		BU::SetBit(&TokenArray[token], offset);
	}

	/// Remove one element 
	// O(1)
	void Remove(TElement element)
	{
		assert(element < Tokens*ElementsPerToken);
		assert(element < MaxElements);

		// Optmizer will reduce integer divisions to bit shifts
		auto token = element / ElementsPerToken;
		auto offset = element % ElementsPerToken;

		BU::ClearBit(&TokenArray[token], offset);
	}

	/// Check if the Set is empty
	/// O(Nmax) -> Nmax no es el numero de elementos en el conjunto. 
	/// Es el numero MAXIMO de elementos en el conjunto.
	/// Respecto al numero de elementos se ejecuta en tiempo constante
	bool IsEmpty() const
	{
		for(unsigned s=0; s<Tokens; s++)
		{
			if(TokenArray[s]) return false;
		}
		return true;
	}

	/// Get the maximum number of elements
	unsigned GetMaxElements() const
	{
		return MaxElements;
	}

	/// Add every member from other set
	/// O(Nmax) -> Nmax no es el numero de elementos en el conjunto. 
	/// Es el numero MAXIMO de elementos en el conjunto.
	/// Respecto al numero de elementos la complejidad es constante
	void UnionWith(const BitSet& c)
	{
		assert(c.Tokens == Tokens);
		BU::OrVector(TokenArray, TokenArray, c.TokenArray, Tokens);
	}

	/// Remove elements do not contained in both sets
	/// O(Nmax) -> Nmax no es el numero de elementos en el conjunto. 
	/// Es el numero MAXIMO de elementos en el conjunto.
	/// Respecto al numero de elementos la complejidad es constante
	void IntersectWith(const BitSet& c)
	{
		assert(c.Tokens == Tokens);
		BU::AndVector(TokenArray, TokenArray, c.TokenArray, Tokens);
	}

	/// Calculates the complement
	void Complement()
	{
		for(unsigned s=0; s<Tokens; s++)
		{
			TokenArray[s] = ~TokenArray[s];
		}
	}
	
	/// Iterates for each member and applies the function <param ref="cb" />
	/// If function applied return false, iteration is stopped.
	/// Enumerates in O(N) -> N es el numero de elementos
	void ForEachMember(std::function< bool(TElement) > cb) const
	{	
		int offset = 0;
		for(unsigned i=0; i<Tokens; i++) // Tokens can not change, it is a O(1) contributing loop
		{
			auto token = TokenArray[i];
			if(i == Tokens-1)  // skip out of range bits
			{
				token &= LastTokenMask;
			}
			unsigned long idx;
			while(BU::BitScanForward(&idx, token)) // each cycle is one member always
			{
				auto st = idx + offset;
				auto cont = cb(st);
				if(!cont) return;
				BU::ClearBit(&token, idx);
			}
			offset += ElementsPerToken;
		}
	}
};
