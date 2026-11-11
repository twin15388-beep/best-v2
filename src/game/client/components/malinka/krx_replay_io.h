// Bounded reader for the observed krxclient/krx-replays 069/003 sample schema.
// This is NOT a reverse-engineered guarantee for every Ultimate 1.32 replay.
// Initial physics state, map hash, position phase and dummy tracks are absent.
// Consumers must keep these inputs preview-only until independent validation.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_KRX_REPLAY_IO_H
#define GAME_CLIENT_COMPONENTS_MALINKA_KRX_REPLAY_IO_H

#include "replay_io.h"

#include <base/vmath.h>
#include <engine/external/json-parser/json.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string_view>

namespace Malinka::KrxReplay
{
// Our resource limits, not claimed limits of KRX's private file format.
constexpr size_t MAX_COMPRESSED_BYTES = 64 * 1024 * 1024;
constexpr uint32_t MAX_EXPANDED_BYTES = 32 * 1024 * 1024;
constexpr unsigned long MAX_JSON_MEMORY = 256 * 1024 * 1024;
constexpr int MAX_JSON_DEPTH = 32;

// The inherited C parser can leave disconnected allocations on second-pass
// OOM. Own every block for this parse, including partial/unreachable trees.
// Its values are POD, so a bounded flat arena needs no recursive destruction.
class CJsonMemory
{
	struct alignas(std::max_align_t) SBlock
	{
		SBlock *m_pNext;
	};
	SBlock *m_pFirst = nullptr;
	size_t m_Allocated = 0;

public:
	CJsonMemory() = default;
	CJsonMemory(const CJsonMemory &) = delete;
	CJsonMemory &operator=(const CJsonMemory &) = delete;
	~CJsonMemory()
	{
		while(m_pFirst)
		{
			auto *pBlock = m_pFirst;
			m_pFirst = pBlock->m_pNext;
			std::free(pBlock);
		}
	}
	static void *Allocate(size_t Size, int Zero, void *pUser) noexcept
	{
		auto &Memory = *static_cast<CJsonMemory *>(pUser);
		if(Memory.m_Allocated > MAX_JSON_MEMORY - sizeof(SBlock) || Size > MAX_JSON_MEMORY - sizeof(SBlock) - Memory.m_Allocated)
			return nullptr;
		auto *pBlock = static_cast<SBlock *>(std::malloc(sizeof(SBlock) + Size));
		if(!pBlock)
			return nullptr;
		pBlock->m_pNext = Memory.m_pFirst;
		Memory.m_pFirst = pBlock;
		Memory.m_Allocated += sizeof(SBlock) + Size;
		if(Zero)
			std::memset(pBlock + 1, 0, Size);
		return pBlock + 1;
	}
	static void Release(void *, void *) noexcept {} // All blocks released at scope exit.
};

struct SInfo
{
	std::string m_Map;
	std::string m_Author;
	std::string m_Description;
	size_t m_NormalizedZeroAimFrames = 0;
};

inline bool Text(const json_value *pValue, size_t Limit, bool AllowEmpty, std::string &Out)
{
	if(!pValue || pValue->type != json_string || pValue->u.string.length > Limit ||
		(!AllowEmpty && pValue->u.string.length == 0))
		return false;
	const std::string_view Value(pValue->u.string.ptr, pValue->u.string.length);
	for(size_t Index = 0; Index < Value.size();)
	{
		const unsigned char First = Value[Index++];
		if(First < 32 || First == 127)
			return false;
		if(First < 128)
			continue;
		int Extra;
		uint32_t Code, Minimum;
		if(First >= 0xc2 && First <= 0xdf)
			Extra = 1, Code = First & 0x1f, Minimum = 0x80;
		else if(First >= 0xe0 && First <= 0xef)
			Extra = 2, Code = First & 0x0f, Minimum = 0x800;
		else if(First >= 0xf0 && First <= 0xf4)
			Extra = 3, Code = First & 7, Minimum = 0x10000;
		else
			return false;
		if(Index + Extra > Value.size())
			return false;
		while(Extra--)
		{
			const unsigned char Next = Value[Index++];
			if((Next & 0xc0) != 0x80)
				return false;
			Code = (Code << 6) | (Next & 0x3f);
		}
		if(Code < Minimum || Code > 0x10ffff || (Code >= 0xd800 && Code <= 0xdfff))
			return false;
	}
	Out.assign(Value);
	return true;
}

inline const json_value *Field(const json_value *pObject, std::string_view Key)
{
	if(!pObject || pObject->type != json_object)
		return nullptr;
	for(unsigned Index = 0; Index < pObject->u.object.length; ++Index)
	{
		const auto &Entry = pObject->u.object.values[Index];
		if(std::string_view(Entry.name, Entry.name_length) == Key)
			return Entry.value;
	}
	return nullptr;
}

inline bool ExactKeys(const json_value *pObject, std::initializer_list<std::string_view> Keys)
{
	if(!pObject || pObject->type != json_object || pObject->u.object.length != Keys.size())
		return false;
	for(unsigned Index = 0; Index < pObject->u.object.length; ++Index)
	{
		const auto &Entry = pObject->u.object.values[Index];
		const std::string_view Name(Entry.name, Entry.name_length);
		if(std::find(Keys.begin(), Keys.end(), Name) == Keys.end())
			return false;
		for(unsigned Previous = 0; Previous < Index; ++Previous)
		{
			const auto &Other = pObject->u.object.values[Previous];
			if(Name == std::string_view(Other.name, Other.name_length))
				return false;
		}
	}
	return true;
}

inline bool Integer(const json_value *pValue, int &Out)
{
	if(!pValue || pValue->type != json_integer || pValue->u.integer < std::numeric_limits<int>::min() ||
		pValue->u.integer > std::numeric_limits<int>::max())
		return false;
	Out = static_cast<int>(pValue->u.integer);
	return true;
}

inline bool Coordinate(const json_value *pValue, float &Out)
{
	if(!pValue || (pValue->type != json_integer && pValue->type != json_double))
		return false;
	const double Value = pValue->type == json_integer ? static_cast<double>(pValue->u.integer) : pValue->u.dbl;
	if(!std::isfinite(Value) || std::abs(Value) > Replay::MAX_COORDINATE)
		return false;
	Out = static_cast<float>(Value);
	return std::isfinite(Out);
}

inline bool BoundedDepth(std::string_view Json)
{
	int Depth = 0;
	bool InString = false, Escape = false;
	for(const char C : Json)
	{
		if(InString)
		{
			if(Escape)
				Escape = false;
			else if(C == '\\')
				Escape = true;
			else if(C == '"')
				InString = false;
		}
		else if(C == '"')
			InString = true;
		else if(C == '{' || C == '[')
		{
			if(++Depth > MAX_JSON_DEPTH)
				return false;
		}
		else if((C == '}' || C == ']') && --Depth < 0)
			return false;
	}
	return !InString && !Escape && Depth == 0;
}

template<typename TFrame>
bool ReadData(std::string_view Encoded, std::vector<TFrame> &vOut, SInfo &InfoOut, std::string &Error) try
{
	if(Encoded.size() <= 4 || Encoded.size() > MAX_COMPRESSED_BYTES)
		return Error = "KRX sample import: invalid compressed size (limit 64 MiB).", false;
	// LE32 expanded length is observed at file offset 0 in both pinned samples.
	// It is not a game offset or a version-independent Ultimate format claim.
	const auto *pBytes = reinterpret_cast<const unsigned char *>(Encoded.data());
	const uint32_t Declared = static_cast<uint32_t>(pBytes[0]) | (static_cast<uint32_t>(pBytes[1]) << 8) |
		(static_cast<uint32_t>(pBytes[2]) << 16) | (static_cast<uint32_t>(pBytes[3]) << 24);
	if(Declared == 0 || Declared > MAX_EXPANDED_BYTES)
		return Error = "Unsupported replay or expanded-size limit (32 MiB). Expected the measured KRX zlib/JSON schema.", false;
	std::string Json(Declared, '\0');
	uLongf Written = Declared;
	uLong Read = static_cast<uLong>(Encoded.size() - 4);
	if(uncompress2(reinterpret_cast<Bytef *>(Json.data()), &Written, pBytes + 4, &Read) != Z_OK ||
		Written != Declared || Read != Encoded.size() - 4 || !BoundedDepth(Json))
		return Error = "KRX sample import: truncated/corrupt zlib, trailing stream, length mismatch or JSON nesting limit.", false;
	CJsonMemory Memory;
	json_settings Settings{};
	Settings.max_memory = MAX_JSON_MEMORY;
	Settings.mem_alloc = CJsonMemory::Allocate;
	Settings.mem_free = CJsonMemory::Release;
	Settings.user_data = &Memory;
	char aJsonError[json_error_max]{};
	const auto *pRoot = json_parse_ex(&Settings, Json.data(), Json.size(), aJsonError);
	if(!pRoot)
		return Error = "KRX sample import: invalid JSON or parser memory limit.", false;
	if(!ExactKeys(pRoot, {"info", "replay_local"}))
		return Error = "Unsupported KRX root schema (unknown fields, duplicate keys or dummy tracks).", false;
	const auto *pInfo = Field(pRoot, "info");
	SInfo Info;
	std::string Date;
	if(!ExactKeys(pInfo, {"author", "date_modified", "description", "map"}) ||
		!Text(Field(pInfo, "map"), 128, false, Info.m_Map) || !Text(Field(pInfo, "author"), 128, true, Info.m_Author) ||
		!Text(Field(pInfo, "description"), 2048, true, Info.m_Description) || !Text(Field(pInfo, "date_modified"), 128, true, Date))
		return Error = "KRX sample import: invalid/unsupported map or metadata schema.", false;
	const auto *pFrames = Field(pRoot, "replay_local");
	if(!pFrames || pFrames->type != json_array || pFrames->u.array.length == 0 || pFrames->u.array.length > Replay::MAX_FRAMES)
		return Error = "KRX sample import: invalid frame count.", false;
	std::vector<TFrame> vFrames;
	vFrames.reserve(pFrames->u.array.length);
	for(unsigned Index = 0; Index < pFrames->u.array.length; ++Index)
	{
		const auto *pFrame = pFrames->u.array.values[Index];
		const auto *pInput = Field(pFrame, "input"), *pPos = Field(pFrame, "pos");
		TFrame Frame{};
		Frame.m_Tick = static_cast<int>(Index); // Relative sequence, not an invented file tick.
		auto &Input = Frame.m_Input;
		if(!ExactKeys(pFrame, {"input", "pos"}) || !ExactKeys(pPos, {"x", "y"}) ||
			!ExactKeys(pInput, {"m_Direction", "m_TargetX", "m_TargetY", "m_Jump", "m_Fire", "m_Hook", "m_PlayerFlags", "m_WantedWeapon", "m_NextWeapon", "m_PrevWeapon"}) ||
			!Integer(Field(pInput, "m_Direction"), Input.m_Direction) || !Integer(Field(pInput, "m_TargetX"), Input.m_TargetX) ||
			!Integer(Field(pInput, "m_TargetY"), Input.m_TargetY) || !Integer(Field(pInput, "m_Jump"), Input.m_Jump) ||
			!Integer(Field(pInput, "m_Fire"), Input.m_Fire) || !Integer(Field(pInput, "m_Hook"), Input.m_Hook) ||
			!Integer(Field(pInput, "m_PlayerFlags"), Input.m_PlayerFlags) || !Integer(Field(pInput, "m_WantedWeapon"), Input.m_WantedWeapon) ||
			!Integer(Field(pInput, "m_NextWeapon"), Input.m_NextWeapon) || !Integer(Field(pInput, "m_PrevWeapon"), Input.m_PrevWeapon) ||
			!Coordinate(Field(pPos, "x"), Frame.m_Pos.x) || !Coordinate(Field(pPos, "y"), Frame.m_Pos.y))
			return Error = "KRX sample import: invalid/unsupported input or position fields.", false;
		// DDNet 18.9.1 (3b4b69c) and this Best base both canonicalize (0,0)
		// to (0,-1) in CCharacter::OnDirectInput AND OnPredictedInput. Keep
		// Malinka's nonzero-aim invariant; disclose the count to the consumer.
		// This does not clamp other inputs or grant physics/playback trust.
		if(Input.m_TargetX == 0 && Input.m_TargetY == 0)
		{
			Input.m_TargetY = -1;
			++Info.m_NormalizedZeroAimFrames;
		}
		if(!Replay::ValidInput(Input))
			return Error = "KRX sample import: input exceeds the supported domain (no clamping).", false;
		Frame.m_Mouse = vec2(Input.m_TargetX, Input.m_TargetY);
		vFrames.push_back(Frame);
	}
	if(!Replay::ValidateFrames(vFrames, Error))
		return false;
	vOut = std::move(vFrames);
	InfoOut = std::move(Info);
	Error.clear();
	return true;
}
catch(const std::exception &)
{
	return Error = "KRX sample import failed (allocation or conversion exception).", false;
}
}

#endif
