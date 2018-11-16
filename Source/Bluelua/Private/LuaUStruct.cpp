#include "LuaUStruct.h"

#include "HAL/UnrealMemory.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "lua.hpp"

const char* FLuaUStruct::USTRUCT_METATABLE = "UStruct_Metatable";

FLuaUStruct::FLuaUStruct(UScriptStruct* InSource, uint8* InScriptBuffer)
	: Source(InSource)
	, ScriptBuffer(InScriptBuffer)
{

}

FLuaUStruct::~FLuaUStruct()
{

}

int32 FLuaUStruct::GetStructureSize() const
{
	return Source ? Source->GetStructureSize() : 0;
}

int FLuaUStruct::Push(lua_State* L, UScriptStruct* InSource, void* InBuffer /*= nullptr*/)
{
	if (!InSource)
	{
		lua_pushnil(L);
		return 1;
	}

	uint8* UserData = (uint8*)lua_newuserdata(L, sizeof(FLuaUStruct));
	uint8* ScriptBuffer = (uint8*)FMemory::Malloc(InSource->GetStructureSize());

	FLuaUStruct* LuaUStruct = new(UserData) FLuaUStruct(InSource, ScriptBuffer);
	InSource->InitializeStruct(ScriptBuffer);
	if (InBuffer)
	{
		InSource->CopyScriptStruct(ScriptBuffer, InBuffer);
	}

	if (luaL_newmetatable(L, USTRUCT_METATABLE))
	{
		static struct luaL_Reg Metamethods[] =
		{
			{ "__index", Index },
			{ "__newindex", NewIndex },
			{ "__gc", GC },
			{ "__tostring", ToString },
			{ NULL, NULL },
		};

		luaL_setfuncs(L, Metamethods, 0);
	}

	lua_setmetatable(L, -2);

	return 1;
}

bool FLuaUStruct::Fetch(lua_State* L, int32 Index, UScriptStruct* OutStruct, uint8* OutBuffer)
{
	if (!OutStruct || lua_isnil(L, Index))
	{
		return false;
	}

	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, Index, FLuaUStruct::USTRUCT_METATABLE);

	//const int32 TargetSize = StructProperty->Struct->GetStructureSize();
	//const int32 SourceSize = LuaUStruct->GetStructureSize();

	//if (TargetSize <= SourceSize)
	{
		OutStruct->CopyScriptStruct(OutBuffer, LuaUStruct->ScriptBuffer);
		return true;
	}
	//else
	//{
	//	UE_LOG(LogBluelua, Warning,
	//		TEXT("Fetch property[%s] struct at index[%d] failed! Structure size mismatch! TargetSize[%d], SourceSize[%d]."),
	//		*Property->GetName(), Index, TargetSize, SourceSize);

	//	return false;
	//}
}

int FLuaUStruct::Index(lua_State* L)
{
	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, 1, USTRUCT_METATABLE);
	if (!LuaUStruct->Source)
	{
		return 0;
	}

	const char* PropertyName = lua_tostring(L, 2);
	if (UProperty* Property = LuaUStruct->Source->FindPropertyByName(PropertyName))
	{
		return FLuaObjectBase::PushProperty(L, Property, LuaUStruct->ScriptBuffer);
	}

	return 0;
}

int FLuaUStruct::NewIndex(lua_State* L)
{
	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, 1, USTRUCT_METATABLE);
	if (!LuaUStruct->Source)
	{
		return 0;
	}

	const char* PropertyName = lua_tostring(L, 2);
	UProperty* Property = LuaUStruct->Source->FindPropertyByName(PropertyName);
	if (Property)
	{
		if (Property->PropertyFlags & CPF_BlueprintReadOnly)
		{
			luaL_error(L, "Can't write to a readonly property[%s] in struct[%s]!", PropertyName, TCHAR_TO_UTF8(*(LuaUStruct->Source->GetName())));
		}

		FetchProperty(L, Property, LuaUStruct->ScriptBuffer, 3);
	}
	else
	{
		luaL_error(L, "Can't find property[%s] in struct[%s]!", PropertyName, TCHAR_TO_UTF8(*(LuaUStruct->Source->GetName())));
	}

	return 0;
}

int FLuaUStruct::GC(lua_State* L)
{
	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, 1, USTRUCT_METATABLE);

	if (LuaUStruct->Source && LuaUStruct->ScriptBuffer)
	{
		LuaUStruct->Source->DestroyStruct(LuaUStruct->ScriptBuffer);
	}

	if (LuaUStruct->ScriptBuffer)
	{
		FMemory::Free(LuaUStruct->ScriptBuffer);
		LuaUStruct->ScriptBuffer = nullptr;
	}

	return 0;
}

int FLuaUStruct::ToString(lua_State* L)
{
	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, 1, USTRUCT_METATABLE);

	lua_pushstring(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("UStruct[%s]"), LuaUStruct->Source ? *(LuaUStruct->Source->GetName()) : TEXT("null"))));

	return 1;
}