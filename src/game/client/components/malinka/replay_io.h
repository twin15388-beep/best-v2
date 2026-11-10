#ifndef GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_IO_H
#define GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_IO_H

#include <generated/protocol.h>
#include "replay_metadata.h"
#include "file_safety.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include <zlib.h>


namespace Malinka::Replay
{
// Resource limits for this file format, not limits reverse-engineered from KRX.
constexpr size_t MAX_FRAMES = 180000;
constexpr size_t MAX_FILE_BYTES = 64 * 1024 * 1024;
constexpr float MAX_COORDINATE = 10000000.0f;
constexpr int MAX_AIM_COMPONENT = 1000000;
constexpr const char *MAGIC = "MALINKATAS2";

inline bool ValidInput(const CNetObj_PlayerInput &Input)
{
	return Input.m_Direction >= -1 && Input.m_Direction <= 1 &&
		(Input.m_TargetX != 0 || Input.m_TargetY != 0) &&
		Input.m_TargetX >= -MAX_AIM_COMPONENT && Input.m_TargetX <= MAX_AIM_COMPONENT &&
		Input.m_TargetY >= -MAX_AIM_COMPONENT && Input.m_TargetY <= MAX_AIM_COMPONENT &&
		Input.m_Jump >= 0 && Input.m_Jump <= 1 && Input.m_Hook >= 0 && Input.m_Hook <= 1 &&
		Input.m_Fire >= 0 && Input.m_Fire <= INPUT_STATE_MASK && Input.m_PlayerFlags >= 0 &&
		Input.m_WantedWeapon >= 0 && Input.m_WantedWeapon <= NUM_WEAPONS &&
		Input.m_NextWeapon >= 0 && Input.m_NextWeapon <= INPUT_STATE_MASK &&
		Input.m_PrevWeapon >= 0 && Input.m_PrevWeapon <= INPUT_STATE_MASK;
}

inline bool ValidText(const std::string &Text, size_t MaxLength)
{
	return !Text.empty() && Text.size() <= MaxLength &&
		std::none_of(Text.begin(), Text.end(), [](unsigned char C) { return C < 32 || C == 127; });
}

inline bool ValidMetadata(const SMetadata &Meta)
{
	return ValidText(Meta.m_Map, 128) && ValidText(Meta.m_Physics, 128) && Meta.m_MapSha256.size() == 64 &&
		std::all_of(Meta.m_MapSha256.begin(), Meta.m_MapSha256.end(), [](char C) { return (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'); });
}

template<typename TFrame>
bool ValidateFrames(const std::vector<TFrame> &vFrames, std::string &Error)
{
	if(vFrames.empty() || vFrames.size() > MAX_FRAMES)
	{
		Error = "Replay must contain 1 to 180000 frames.";
		return false;
	}
	int64_t Previous = -1;
	for(const auto &Frame : vFrames)
	{
		const auto FiniteCoordinate = [](float Value) { return std::isfinite(Value) && std::abs(Value) <= MAX_COORDINATE; };
		if(Frame.m_Tick < 0 || (Previous >= 0 && static_cast<int64_t>(Frame.m_Tick) != Previous + 1) ||
			!ValidInput(Frame.m_Input) || (Frame.m_HasDummy && (!ValidInput(Frame.m_DummyInput) ||
			!FiniteCoordinate(Frame.m_DummyPos.x) || !FiniteCoordinate(Frame.m_DummyPos.y))) || !FiniteCoordinate(Frame.m_Mouse.x) || !FiniteCoordinate(Frame.m_Mouse.y) ||
			std::abs(Frame.m_Mouse.x) > MAX_AIM_COMPONENT || std::abs(Frame.m_Mouse.y) > MAX_AIM_COMPONENT ||
			!FiniteCoordinate(Frame.m_Pos.x) || !FiniteCoordinate(Frame.m_Pos.y))
		{
			Error = "Invalid input, non-finite/out-of-range coordinate, or non-contiguous frame tick.";
			return false;
		}
		Previous = Frame.m_Tick;
	}
	Error.clear();
	return true;
}

inline void WriteInput(std::ostream &Out, const CNetObj_PlayerInput &Input)
{
	Out << Input.m_Direction << ' ' << Input.m_TargetX << ' ' << Input.m_TargetY << ' '
		<< Input.m_Jump << ' ' << Input.m_Fire << ' ' << Input.m_Hook << ' ' << Input.m_PlayerFlags << ' '
		<< Input.m_WantedWeapon << ' ' << Input.m_NextWeapon << ' ' << Input.m_PrevWeapon;
}

inline void ReadInput(std::istream &In, CNetObj_PlayerInput &Input)
{
	In >> Input.m_Direction >> Input.m_TargetX >> Input.m_TargetY >> Input.m_Jump >> Input.m_Fire >> Input.m_Hook
		>> Input.m_PlayerFlags >> Input.m_WantedWeapon >> Input.m_NextWeapon >> Input.m_PrevWeapon;
}

template<typename TFrame>
bool Read(const std::filesystem::path &Path, std::vector<TFrame> &vOut, SMetadata &MetadataOut, std::string &Error)
{
	std::error_code Ec;
	const auto Size = std::filesystem::file_size(Path, Ec);
	if(Ec || Size == 0 || Size > MAX_FILE_BYTES)
	{
		Error = "Replay is missing, unreadable, empty, or exceeds 64 MiB.";
		return false;
	}
	std::ifstream File(Path, std::ios::binary);
	std::string Magic, ChecksumLine;
	if(!std::getline(File, Magic) || Magic.size() > 32)
		return Error = "Invalid replay header.", false;
	SMetadata Meta;
	Meta.m_Legacy = Magic == "PSTATAS2";
	if(!Meta.m_Legacy && Magic != MAGIC)
		return Error = "Unsupported format. Official KRX binary .tas files are not supported.", false;
	uint32_t ExpectedCrc = 0;
	if(!Meta.m_Legacy)
	{
		if(!std::getline(File, ChecksumLine) || ChecksumLine.size() != 14 || ChecksumLine.substr(0, 6) != "crc32 ")
			return Error = "Missing replay checksum.", false;
		const std::string Hex = ChecksumLine.substr(6);
		if(!std::all_of(Hex.begin(), Hex.end(), [](char C) { return (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'); }))
			return Error = "Invalid replay checksum.", false;
		std::istringstream Checksum(Hex);
		Checksum >> std::hex >> ExpectedCrc;
	}
	const std::string Body((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	if(File.bad() || Body.size() > MAX_FILE_BYTES)
		return Error = "Replay read failed.", false;
	if(!Meta.m_Legacy && crc32(0, reinterpret_cast<const Bytef *>(Body.data()), static_cast<uInt>(Body.size())) != ExpectedCrc)
		return Error = "Replay checksum mismatch.", false;
	std::istringstream In(Body);
	In.imbue(std::locale::classic());
	std::string Key;
	if(!Meta.m_Legacy)
	{
		if(!(In >> Key >> std::quoted(Meta.m_Physics)) || Key != "physics" ||
			!(In >> Key >> std::quoted(Meta.m_Map)) || Key != "map" ||
			!(In >> Key >> Meta.m_MapSha256) || Key != "map_sha256" || !ValidMetadata(Meta))
			return Error = "Invalid replay map/physics metadata.", false;
	}
	size_t Count = 0;
	if(!(In >> Key >> Count) || Key != "frames" || Count == 0 || Count > MAX_FRAMES)
		return Error = "Invalid replay frame count.", false;
	std::string Line;
	std::getline(In, Line);
	if(Line.find_first_not_of(" \t\r") != std::string::npos)
		return Error = "Trailing frame-header data.", false;
	std::vector<TFrame> vFrames;
	vFrames.reserve(Count);
	for(size_t Index = 0; Index < Count; ++Index)
	{
		if(!std::getline(In, Line) || Line.size() > 1024)
			return Error = "Truncated replay or oversized frame.", false;
		std::istringstream Row(Line);
		Row.imbue(std::locale::classic());
		TFrame Frame{};
		Row >> Frame.m_Tick;
		ReadInput(Row, Frame.m_Input);
		Row >> Frame.m_Mouse.x >> Frame.m_Mouse.y >> Frame.m_Pos.x >> Frame.m_Pos.y;
		if(!Meta.m_Legacy)
		{
			int HasDummy = 0;
			Row >> HasDummy;
			if(HasDummy < 0 || HasDummy > 1)
				return Error = "Invalid dummy flag.", false;
			Frame.m_HasDummy = HasDummy != 0;
			if(Frame.m_HasDummy)
			{
				ReadInput(Row, Frame.m_DummyInput);
				Row >> Frame.m_DummyPos.x >> Frame.m_DummyPos.y;
			}
		}
		if(Row.fail())
			return Error = "Invalid replay frame fields.", false;
		Row >> std::ws;
		if(!Row.eof())
			return Error = "Trailing replay frame fields.", false;
		vFrames.push_back(Frame);
	}
	In >> std::ws;
	if(!In.eof())
		return Error = "Replay count does not match its content.", false;
	if(!ValidateFrames(vFrames, Error))
		return false;
	vOut = std::move(vFrames); // A failed read never mutates the caller's current replay.
	MetadataOut = std::move(Meta);
	return true;
}

template<typename TFrame>
bool Write(const std::filesystem::path &Path, const std::vector<TFrame> &vFrames, const SMetadata &Meta, std::string &Error)
{
	if(!ValidMetadata(Meta) || !ValidateFrames(vFrames, Error))
		return Error = Error.empty() ? "Missing map identity." : Error, false;
	std::ostringstream Payload;
	Payload.imbue(std::locale::classic());
	Payload << std::setprecision(std::numeric_limits<float>::max_digits10)
		<< "physics " << std::quoted(Meta.m_Physics) << '\n'
		<< "map " << std::quoted(Meta.m_Map) << '\n' << "map_sha256 " << Meta.m_MapSha256 << '\n'
		<< "frames " << vFrames.size() << '\n';
	for(const auto &Frame : vFrames)
	{
		Payload << Frame.m_Tick << ' ';
		WriteInput(Payload, Frame.m_Input);
		Payload << ' ' << Frame.m_Mouse.x << ' ' << Frame.m_Mouse.y << ' ' << Frame.m_Pos.x << ' ' << Frame.m_Pos.y << ' ' << static_cast<int>(Frame.m_HasDummy);
		if(Frame.m_HasDummy)
		{
			Payload << ' ';
			WriteInput(Payload, Frame.m_DummyInput);
			Payload << ' ' << Frame.m_DummyPos.x << ' ' << Frame.m_DummyPos.y;
		}
		Payload << '\n';
	}
	const std::string Body = Payload.str();
	if(Body.size() > MAX_FILE_BYTES - 64)
		return Error = "Replay exceeds the file-size limit.", false;
	const auto Temp = std::filesystem::path(Path).concat(".new");
	const auto Backup = std::filesystem::path(Path).concat(".bak");
	const auto BackupTemp = std::filesystem::path(Backup).concat(".new");
	if(!Files::NoLinks(Path) || !Files::NoLinks(Temp) || !Files::NoLinks(Backup) || !Files::NoLinks(BackupTemp))
		return Error = "Replay path/temporary/backup contains a link; no file was replaced.", false;
	std::ostringstream Encoded;
	Encoded << MAGIC << '\n' << "crc32 " << std::hex << std::setw(8) << std::setfill('0')
		<< crc32(0, reinterpret_cast<const Bytef *>(Body.data()), static_cast<uInt>(Body.size())) << '\n' << Body;
	if(!Files::WriteExclusive(Temp, Encoded.str()))
		return Error = "Cannot exclusively create/flush replay .new file. Existing files were not replaced.", false;
	const auto RemoveTemp = [&] {
		std::error_code Ignored;
		std::filesystem::remove(Temp, Ignored);
	};
	std::vector<TFrame> vCheck;
	SMetadata CheckMeta;
	if(!Read(Temp, vCheck, CheckMeta, Error))
	{
		RemoveTemp();
		return false;
	}
	std::error_code Ec;
	const bool Exists = std::filesystem::exists(Path, Ec);
	if(Ec)
	{
		RemoveTemp();
		return Error = "Cannot inspect existing replay; replacement cancelled.", false;
	}
	if(Exists)
	{
		std::string OldData;
		if(!Files::ReadBounded(Path, MAX_FILE_BYTES, OldData) || !Files::WriteExclusive(BackupTemp, OldData))
		{
			RemoveTemp();
			return Error = "Cannot stage a complete backup of the existing replay; replacement cancelled.", false;
		}
		if(!Files::AtomicReplace(BackupTemp, Backup))
		{
			std::filesystem::remove(BackupTemp, Ec);
			RemoveTemp();
			return Error = "Cannot install replay backup; replacement cancelled.", false;
		}
	}
	if(!Files::NoLinks(Path) || !Files::AtomicReplace(Temp, Path))
	{
		RemoveTemp();
		return Error = "Cannot replace replay; old file and backup were retained.", false;
	}
	Error.clear();
	return true;
}
}

#endif
