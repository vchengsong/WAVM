#pragma once

#include "IR.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "Inline/Floats.h"
#include "Inline/Hash.h"

#include <string>
#include <vector>

namespace IR
{
	// The type of a WebAssembly operand
	enum class ValueType : U8
	{
		any  = 0,
		i32  = 1,
		i64  = 2,
		f32  = 3,
		f64  = 4,
		v128 = 5,

		num,
		max = num - 1
	};

	template<ValueType type> struct ValueTypeInfo;
	template<> struct ValueTypeInfo<ValueType::i32>
	{
		typedef I32 Value;
	};
	template<> struct ValueTypeInfo<ValueType::i64>
	{
		typedef I64 Value;
	};
	template<> struct ValueTypeInfo<ValueType::f32>
	{
		typedef F32 Value;
	};
	template<> struct ValueTypeInfo<ValueType::f64>
	{
		typedef F64 Value;
	};

	inline std::string asString(I32 value) { return std::to_string(value); }
	inline std::string asString(I64 value) { return std::to_string(value); }
	inline std::string asString(F32 value) { return Floats::asString(value); }
	inline std::string asString(F64 value) { return Floats::asString(value); }

	inline std::string asString(const V128& v128)
	{
		// buffer needs 44 characters:
		// 0xHHHHHHHH 0xHHHHHHHH 0xHHHHHHHH 0xHHHHHHHH\0
		char buffer[44];
		snprintf(buffer,
				 sizeof(buffer),
				 "0x%.8x 0x%.8x 0x%.8x 0x%.8x",
				 v128.u32[0],
				 v128.u32[1],
				 v128.u32[2],
				 v128.u32[3]);
		return std::string(buffer);
	}

	template<> struct ValueTypeInfo<ValueType::v128>
	{
		typedef V128 Value;
	};

	inline U8 getTypeByteWidth(ValueType type)
	{
		switch(type)
		{
		case ValueType::i32: return 4;
		case ValueType::i64: return 8;
		case ValueType::f32: return 4;
		case ValueType::f64: return 8;
		case ValueType::v128: return 16;
		default: Errors::unreachable();
		};
	}

	inline U8 getTypeBitWidth(ValueType type) { return getTypeByteWidth(type) * 8; }

	inline const char* asString(ValueType type)
	{
		switch(type)
		{
		case ValueType::any: return "any";
		case ValueType::i32: return "i32";
		case ValueType::i64: return "i64";
		case ValueType::f32: return "f32";
		case ValueType::f64: return "f64";
		case ValueType::v128: return "v128";
		default: Errors::unreachable();
		};
	}

	// The tuple of value types.
	struct TypeTuple
	{
		IR_API TypeTuple();
		IR_API TypeTuple(ValueType inElem);
		IR_API TypeTuple(const std::initializer_list<ValueType>& inElems);
		IR_API TypeTuple(const std::vector<ValueType>& inElems);

		const ValueType* begin() const { return impl->elems; }
		const ValueType* end() const { return impl->elems + impl->numElems; }
		const ValueType* data() const { return impl->elems; }

		ValueType operator[](Uptr index) const
		{
			wavmAssert(index < impl->numElems);
			return impl->elems[index];
		}

		Uptr getHash() const { return impl->hash; }
		Uptr size() const { return impl->numElems; }

		friend bool operator==(const TypeTuple& left, const TypeTuple& right)
		{
			return left.impl == right.impl;
		}
		friend bool operator!=(const TypeTuple& left, const TypeTuple& right)
		{
			return left.impl != right.impl;
		}

	private:
		struct Impl
		{
			Uptr hash;
			Uptr numElems;
			ValueType elems[1];

			Impl(Uptr inNumElems, const ValueType* inElems);
			Impl(const Impl& inCopy);

			static Uptr calcNumBytes(Uptr numElems)
			{
				return offsetof(Impl, elems) + numElems * sizeof(ValueType);
			}
		};

		const Impl* impl;

		TypeTuple(const Impl* inImpl) : impl(inImpl) {}

		IR_API static const Impl* getUniqueImpl(Uptr numElems, const ValueType* inElems);
	};

	inline std::string asString(TypeTuple typeTuple)
	{
		if(typeTuple.size() == 1) { return asString(typeTuple[0]); }
		else
		{
			std::string result = "(";
			for(Uptr elementIndex = 0; elementIndex < typeTuple.size(); ++elementIndex)
			{
				if(elementIndex != 0) { result += ", "; }
				result += asString(typeTuple[elementIndex]);
			}
			result += ")";
			return result;
		}
	}

	// Infer value and result types from a C type.

	template<typename> constexpr ValueType inferValueType();
	template<> constexpr ValueType inferValueType<I32>() { return ValueType::i32; }
	template<> constexpr ValueType inferValueType<U32>() { return ValueType::i32; }
	template<> constexpr ValueType inferValueType<I64>() { return ValueType::i64; }
	template<> constexpr ValueType inferValueType<U64>() { return ValueType::i64; }
	template<> constexpr ValueType inferValueType<F32>() { return ValueType::f32; }
	template<> constexpr ValueType inferValueType<F64>() { return ValueType::f64; }

	template<typename T> inline TypeTuple inferResultType()
	{
		return TypeTuple(inferValueType<T>());
	}
	template<> inline TypeTuple inferResultType<void>() { return TypeTuple(); }

	// The type of a WebAssembly function
	struct FunctionType
	{
		// Used to represent a function type as an abstract pointer-sized value in the runtime.
		struct Encoding
		{
			Uptr impl;
		};

		FunctionType(TypeTuple inResults = TypeTuple(), TypeTuple inParams = TypeTuple())
		: impl(getUniqueImpl(inResults, inParams))
		{
		}

		FunctionType(Encoding encoding) : impl(reinterpret_cast<const Impl*>(encoding.impl)) {}

		TypeTuple results() const { return impl->results; }
		TypeTuple params() const { return impl->params; }
		Uptr getHash() const { return impl->hash; }
		Encoding getEncoding() const { return Encoding{reinterpret_cast<Uptr>(impl)}; }

		friend bool operator==(const FunctionType& left, const FunctionType& right)
		{
			return left.impl == right.impl;
		}

		friend bool operator!=(const FunctionType& left, const FunctionType& right)
		{
			return left.impl != right.impl;
		}

	private:
		struct Impl
		{
			Uptr hash;
			TypeTuple results;
			TypeTuple params;

			Impl(TypeTuple inResults, TypeTuple inParams);
		};

		const Impl* impl;

		FunctionType(const Impl* inImpl) : impl(inImpl) {}

		IR_API static const Impl* getUniqueImpl(TypeTuple results, TypeTuple params);
	};

	struct IndexedFunctionType
	{
		Uptr index;
	};

	struct IndexedBlockType
	{
		enum Format
		{
			noParametersOrResult,
			oneResult,
			functionType
		};
		Format format;
		union
		{
			ValueType resultType;
			Uptr index;
		};
	};

	inline std::string asString(const FunctionType& functionType)
	{
		return asString(functionType.params()) + "->" + asString(functionType.results());
	}

	// A size constraint: a range of expected sizes for some size-constrained type.
	// If max==UINT64_MAX, the maximum size is unbounded.
	struct SizeConstraints
	{
		U64 min;
		U64 max;

		friend bool operator==(const SizeConstraints& left, const SizeConstraints& right)
		{
			return left.min == right.min && left.max == right.max;
		}
		friend bool operator!=(const SizeConstraints& left, const SizeConstraints& right)
		{
			return left.min != right.min || left.max != right.max;
		}
		friend bool isSubset(const SizeConstraints& super, const SizeConstraints& sub)
		{
			return sub.min >= super.min && sub.max <= super.max;
		}
	};

	inline std::string asString(const SizeConstraints& sizeConstraints)
	{
		return std::to_string(sizeConstraints.min)
			   + (sizeConstraints.max == UINT64_MAX ? ".."
													: ".." + std::to_string(sizeConstraints.max));
	}

	// The type of element a table contains: for now can only be anyfunc, meaning any function type.
	enum class TableElementType : U8
	{
		anyfunc = 0x70
	};

	// The type of a table
	struct TableType
	{
		TableElementType elementType;
		bool isShared;
		SizeConstraints size;

		TableType() : elementType(TableElementType::anyfunc), isShared(false), size({0, UINT64_MAX})
		{
		}
		TableType(TableElementType inElementType, bool inIsShared, SizeConstraints inSize)
		: elementType(inElementType), isShared(inIsShared), size(inSize)
		{
		}

		friend bool operator==(const TableType& left, const TableType& right)
		{
			return left.elementType == right.elementType && left.isShared == right.isShared
				   && left.size == right.size;
		}
		friend bool operator!=(const TableType& left, const TableType& right)
		{
			return left.elementType != right.elementType || left.isShared != right.isShared
				   || left.size != right.size;
		}
		friend bool isSubset(const TableType& super, const TableType& sub)
		{
			return super.elementType == sub.elementType && super.isShared == sub.isShared
				   && isSubset(super.size, sub.size);
		}
	};

	inline std::string asString(const TableType& tableType)
	{
		return asString(tableType.size) + (tableType.isShared ? " shared anyfunc" : " anyfunc");
	}

	// The type of a memory
	struct MemoryType
	{
		bool isShared;
		SizeConstraints size;

		MemoryType() : isShared(false), size({0, UINT64_MAX}) {}
		MemoryType(bool inIsShared, const SizeConstraints& inSize)
		: isShared(inIsShared), size(inSize)
		{
		}

		friend bool operator==(const MemoryType& left, const MemoryType& right)
		{
			return left.isShared == right.isShared && left.size == right.size;
		}
		friend bool operator!=(const MemoryType& left, const MemoryType& right)
		{
			return left.isShared != right.isShared || left.size != right.size;
		}
		friend bool isSubset(const MemoryType& super, const MemoryType& sub)
		{
			return super.isShared == sub.isShared && isSubset(super.size, sub.size);
		}
	};

	inline std::string asString(const MemoryType& memoryType)
	{
		return asString(memoryType.size) + (memoryType.isShared ? " shared" : "");
	}

	// The type of a global
	struct GlobalType
	{
		ValueType valueType;
		bool isMutable;

		GlobalType() : valueType(ValueType::any), isMutable(false) {}
		GlobalType(ValueType inValueType, bool inIsMutable)
		: valueType(inValueType), isMutable(inIsMutable)
		{
		}

		friend bool operator==(const GlobalType& left, const GlobalType& right)
		{
			return left.valueType == right.valueType && left.isMutable == right.isMutable;
		}
		friend bool operator!=(const GlobalType& left, const GlobalType& right)
		{
			return left.valueType != right.valueType || left.isMutable != right.isMutable;
		}
		friend bool operator<=(const GlobalType& left, const GlobalType& right)
		{
			return left.valueType == right.valueType && left.isMutable == right.isMutable;
		}
		friend bool operator>(const GlobalType& left, const GlobalType& right)
		{
			return !(left <= right);
		}
	};

	inline std::string asString(const GlobalType& globalType)
	{
		if(globalType.isMutable) { return std::string("global ") + asString(globalType.valueType); }
		else
		{
			return std::string("immutable ") + asString(globalType.valueType);
		}
	}

	struct ExceptionType
	{
		TypeTuple params;

		friend bool operator==(const ExceptionType& left, const ExceptionType& right)
		{
			return left.params == right.params;
		}
		friend bool operator!=(const ExceptionType& left, const ExceptionType& right)
		{
			return left.params != right.params;
		}
	};

	// The type of an object
	enum class ObjectKind : U8
	{
		// Standard object kinds that may be imported/exported from WebAssembly modules.
		function      = 0,
		table         = 1,
		memory        = 2,
		global        = 3,
		exceptionType = 4,
		max           = 4,
		invalid       = 0xff,
	};
	struct ObjectType
	{
		const ObjectKind kind;

		ObjectType() : kind(ObjectKind::invalid) {}
		ObjectType(FunctionType inFunction) : kind(ObjectKind::function), function(inFunction) {}
		ObjectType(TableType inTable) : kind(ObjectKind::table), table(inTable) {}
		ObjectType(MemoryType inMemory) : kind(ObjectKind::memory), memory(inMemory) {}
		ObjectType(GlobalType inGlobal) : kind(ObjectKind::global), global(inGlobal) {}
		ObjectType(ExceptionType inExceptionType)
		: kind(ObjectKind::exceptionType), exceptionType(inExceptionType)
		{
		}
		ObjectType(ObjectKind inKind) : kind(inKind) {}

		friend FunctionType asFunctionType(const ObjectType& objectType)
		{
			wavmAssert(objectType.kind == ObjectKind::function);
			return objectType.function;
		}
		friend TableType asTableType(const ObjectType& objectType)
		{
			wavmAssert(objectType.kind == ObjectKind::table);
			return objectType.table;
		}
		friend MemoryType asMemoryType(const ObjectType& objectType)
		{
			wavmAssert(objectType.kind == ObjectKind::memory);
			return objectType.memory;
		}
		friend GlobalType asGlobalType(const ObjectType& objectType)
		{
			wavmAssert(objectType.kind == ObjectKind::global);
			return objectType.global;
		}
		friend ExceptionType asExceptionType(const ObjectType& objectType)
		{
			wavmAssert(objectType.kind == ObjectKind::exceptionType);
			return objectType.exceptionType;
		}

	private:
		union
		{
			FunctionType function;
			TableType table;
			MemoryType memory;
			GlobalType global;
			ExceptionType exceptionType;
		};
	};

	inline std::string asString(const ObjectType& objectType)
	{
		switch(objectType.kind)
		{
		case ObjectKind::function: return "func " + asString(asFunctionType(objectType));
		case ObjectKind::table: return "table " + asString(asTableType(objectType));
		case ObjectKind::memory: return "memory " + asString(asMemoryType(objectType));
		case ObjectKind::global: return asString(asGlobalType(objectType));
		case ObjectKind::exceptionType:
			return "exception_type " + asString(asExceptionType(objectType));
		default: Errors::unreachable();
		};
	}
}

template<> struct Hash<IR::TypeTuple>
{
	Uptr operator()(IR::TypeTuple typeTuple, Uptr seed = 0) const
	{
		return Hash<Uptr>()(typeTuple.getHash(), seed);
	}
};

template<> struct Hash<IR::FunctionType>
{
	Uptr operator()(IR::FunctionType functionType, Uptr seed = 0) const
	{
		return Hash<Uptr>()(functionType.getHash(), seed);
	}
};
